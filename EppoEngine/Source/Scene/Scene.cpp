#include "pch.h"
#include "Scene/Scene.h"

#include "Physics/PhysicsWorld.h"
#include "Project/Project.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scripting/ScriptEngine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>

namespace Eppo
{
    static constexpr glm::vec3 s_DefaultGravity = { 0.0f, -9.81f, 0.0f };

    namespace
    {
        struct DecomposedTransform
        {
            glm::vec3 Translation;
            glm::quat Rotation;
            glm::vec3 Scale;
        };

        auto Decompose(const glm::mat4& transform) -> DecomposedTransform
        {
            DecomposedTransform result{};
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transform, result.Scale, result.Rotation, result.Translation, skew, perspective);
            return result;
        }

        // Translates entity's colliders into body-local shapes; sourceToBody must be shear-free (rotation under non-uniform scale is
        // approximated).
        auto AppendColliders(Entity entity, const glm::mat4& sourceToBody, std::vector<ColliderData>& colliders) -> void
        {
            const auto [translation, rotation, rawScale] = Decompose(sourceToBody);
            const glm::vec3 scale = glm::abs(rawScale);
            const auto shapeOffset = [&](const glm::vec3& offset)
            {
                return glm::vec3(sourceToBody * glm::vec4(offset, 1.0f));
            };

            if (entity.HasComponent<BoxColliderComponent>())
            {
                const auto& c = entity.GetComponent<BoxColliderComponent>();
                ColliderData data{ .Shape = ColliderShape::Box,
                                   .Offset = shapeOffset(c.Offset),
                                   .Rotation = rotation,
                                   .Density = c.Density,
                                   .Friction = c.Friction,
                                   .Restitution = c.Restitution };
                data.HalfExtents = c.HalfSize * scale;
                colliders.push_back(data);
            }

            if (entity.HasComponent<SphereColliderComponent>())
            {
                const auto& c = entity.GetComponent<SphereColliderComponent>();
                ColliderData data{ .Shape = ColliderShape::Sphere,
                                   .Offset = shapeOffset(c.Offset),
                                   .Rotation = rotation,
                                   .Density = c.Density,
                                   .Friction = c.Friction,
                                   .Restitution = c.Restitution };
                data.Radius = c.Radius * std::max({ scale.x, scale.y, scale.z });
                colliders.push_back(data);
            }

            if (entity.HasComponent<CapsuleColliderComponent>())
            {
                const auto& c = entity.GetComponent<CapsuleColliderComponent>();
                ColliderData data{ .Shape = ColliderShape::Capsule,
                                   .Offset = shapeOffset(c.Offset),
                                   .Rotation = rotation,
                                   .Density = c.Density,
                                   .Friction = c.Friction,
                                   .Restitution = c.Restitution };
                data.Radius = c.Radius * std::max(scale.x, scale.z);
                data.Height = c.Height * scale.y;
                colliders.push_back(data);
            }

            if (entity.HasComponent<CylinderColliderComponent>())
            {
                const auto& c = entity.GetComponent<CylinderColliderComponent>();
                ColliderData data{ .Shape = ColliderShape::Cylinder,
                                   .Offset = shapeOffset(c.Offset),
                                   .Rotation = rotation,
                                   .Density = c.Density,
                                   .Friction = c.Friction,
                                   .Restitution = c.Restitution };
                data.Radius = c.Radius * std::max(scale.x, scale.z);
                data.Height = c.Height * scale.y;
                colliders.push_back(data);
            }
        }

        // Collects source's subtree colliders, stopping at nested physics roots; the visited set terminates cyclic serialized hierarchies.
        auto GatherColliders(
            Scene& scene, Entity source, const glm::mat4& invBodyPose, std::vector<ColliderData>& colliders,
            std::unordered_set<UUID>& visited
        ) -> void
        {
            if (!visited.insert(source.GetUUID()).second)
            {
                Log::Error("Cycle detected while gathering colliders at entity '{}'; aborting walk.", source.GetName());
                return;
            }

            AppendColliders(source, invBodyPose * scene.GetWorldTransform(source), colliders);

            if (!source.HasComponent<RelationshipComponent>())
                return;

            for (const UUID childId : source.GetComponent<RelationshipComponent>().Children)
            {
                const Entity child = scene.GetEntityByUUID(childId);
                if (!child)
                    continue;

                if (child.HasComponent<RigidBodyComponent>())
                    continue; // A new physics root; its subtree belongs to its own body.

                GatherColliders(scene, child, invBodyPose, colliders, visited);
            }
        }

        auto TryGetMeshBounds(Entity entity, AABB& bounds) -> bool
        {
            if (!entity.HasComponent<MeshComponent>() || !entity.GetComponent<MeshComponent>().MeshHandle)
                return false;

            const AssetHandle meshHandle = entity.GetComponent<MeshComponent>().MeshHandle;
            const uint64_t rawMeshHandle = static_cast<uint64_t>(meshHandle);
            if (rawMeshHandle >= static_cast<uint64_t>(MeshPrimitiveType::Cone) &&
                rawMeshHandle <= static_cast<uint64_t>(MeshPrimitiveType::Capsule))
            {
                bounds.Min =
                    rawMeshHandle == static_cast<uint64_t>(MeshPrimitiveType::Capsule) ? glm::vec3(-1.0f, -2.0f, -1.0f) : glm::vec3(-1.0f);
                bounds.Max = -bounds.Min;
                return true;
            }

            const Ref<Project>& project = Project::GetActive();
            if (!project || !project->GetAssetManager())
                return false;

            const Ref<Mesh> mesh = project->GetAssetManager()->GetOrLoadAsset<Mesh>(meshHandle);
            if (!mesh || !mesh->GetBounds().IsValid())
                return false;

            bounds = mesh->GetBounds();
            return true;
        }
    }

    auto Scene::SetViewportSize(const uint32_t width, const uint32_t height) -> void
    {
        // Keep every scene camera's projection aspect ratio in sync with the viewport.
        for (const auto view = m_Registry.view<CameraComponent>(); const auto e : view)
            view.get<CameraComponent>(e).Camera.SetViewportSize(width, height);
    }

    auto Scene::OnRuntimeStart() -> void
    {
        EP_PROFILE_FN("Scene::OnRuntimeStart");

        // Warn once at play start rather than every frame in OnRenderRuntime.
        if (!GetPrimaryCameraEntity())
            Log::Warn("Scene has no primary camera entity; nothing will be rendered in play mode.");

        // Build the physics world regardless of scripting so it simulates even with no scripts.
        m_PhysicsWorld = CreateRef<PhysicsWorld>(s_DefaultGravity);
        m_ColliderlessRigidBodies.clear();
        {
            for (const auto view = m_Registry.view<RigidBodyComponent, TransformComponent>(); const auto e : view)
            {
                Entity entity(e, this);

                // Scale is not part of the body pose; it flows into the gathered shape dimensions instead.
                const DecomposedTransform bodyTransform = Decompose(GetWorldTransform(entity));
                const glm::vec3& translation = bodyTransform.Translation;
                const glm::quat& rotation = bodyTransform.Rotation;
                const glm::mat4 bodyPose = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);

                std::vector<ColliderData> colliders;
                std::unordered_set<UUID> visited;
                GatherColliders(*this, entity, glm::inverse(bodyPose), colliders, visited);

                if (colliders.empty())
                {
                    Log::Warn("Entity '{}': rigid body has no collider and will still be simulated.", entity.GetName());
                    m_ColliderlessRigidBodies.push_back(entity.GetName());
                }

                m_PhysicsWorld->CreateBody(entity.GetUUID(), entity.GetComponent<RigidBodyComponent>(), translation, rotation, colliders);
            }
        }

        if (!ScriptEngine::IsInitialized())
            return;

        auto& scriptEngine = ScriptEngine::Get();
        scriptEngine.SetActivePhysicsWorld(m_PhysicsWorld);

        // Both contexts must be published before OnCreate: every internal call
        // resolves through them.
        scriptEngine.SetSceneContext(shared_from_this());

        // Snapshot: OnCreate can spawn or destroy scripted entities, mutating the
        // storage this walks. Hence the per-entity validity re-check too.
        const auto scripts = m_Registry.view<ScriptComponent>();
        for (const std::vector scripted(scripts.begin(), scripts.end()); const auto e : scripted)
        {
            if (!m_Registry.valid(e))
                continue;

            const Entity entity(e, this);
            scriptEngine.OnCreateEntity(entity);
        }
    }

    auto Scene::OnRuntimeStop() -> void
    {
        EP_PROFILE_FN("Scene::OnRuntimeStop");

        // OnDestroy runs before either context is torn down, so a script can still
        // reach its entity and the simulation while it cleans up.
        if (ScriptEngine::IsInitialized())
        {
            auto& scriptEngine = ScriptEngine::Get();
            const auto view = m_Registry.view<ScriptComponent>();

            for (const std::vector scripted(view.begin(), view.end()); const auto e : scripted)
            {
                if (!m_Registry.valid(e))
                    continue;

                const Entity entity(e, this);
                scriptEngine.OnDestroyEntity(entity);
            }

            // Only release a context this scene published, so stopping one scene can't
            // yank it from another that is still running.
            if (scriptEngine.GetSceneContext() == shared_from_this())
                scriptEngine.SetSceneContext(nullptr);
        }

        // The script engine's WeakRef expires with this reset.
        m_PhysicsWorld.reset();
        m_ColliderlessRigidBodies.clear();
    }

    auto Scene::OnUpdateRuntime(float timestep) -> void
    {
        EP_PROFILE_FN("Scene::OnUpdateRuntime");

        // Step physics before scripts so they observe this frame's poses (and before
        // the scripting early-return so it runs without scripts).
        if (m_PhysicsWorld)
        {
            m_PhysicsWorld->Step(timestep);

            // Parents first: a parented body's local conversion reads ancestor transforms that must already hold this frame's pose.
            const auto depthOf = [this](const Entity entity) -> size_t
            {
                size_t depth = 0;
                UUID parentId =
                    entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Parent : UUID(0);
                while (parentId && depth <= m_EntityMap.size())
                {
                    const Entity parent = GetEntityByUUID(parentId);
                    if (!parent)
                        break;

                    ++depth;
                    parentId = parent.HasComponent<RelationshipComponent>() ? parent.GetComponent<RelationshipComponent>().Parent : UUID(0);
                }
                return depth;
            };

            std::vector<std::pair<size_t, Entity>> bodies;
            for (const auto view = m_Registry.view<RigidBodyComponent, TransformComponent>(); const auto e : view)
            {
                Entity entity(e, this);
                if (m_PhysicsWorld->HasBody(entity.GetUUID()))
                    bodies.emplace_back(depthOf(entity), entity);
            }
            std::sort(
                bodies.begin(), bodies.end(),
                [](const auto& lhs, const auto& rhs) -> auto
                {
                    return lhs.first < rhs.first;
                }
            );

            for (auto& entity : bodies | std::views::values)
            {
                const UUID id = entity.GetUUID();
                const glm::vec3 position = m_PhysicsWorld->GetPosition(id);
                const glm::quat rotation = m_PhysicsWorld->GetRotation(id);

                auto& tc = entity.GetComponent<TransformComponent>();
                const UUID parentId =
                    entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Parent : UUID(0);
                if (const Entity parent = parentId ? GetEntityByUUID(parentId) : Entity{})
                {
                    // The physics pose is world space; convert back through the parent chain (scale stays authored).
                    const glm::mat4 pose = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
                    const auto local = Decompose(glm::inverse(GetWorldTransform(parent)) * pose);
                    tc.Translation = local.Translation;
                    tc.Rotation = glm::eulerAngles(local.Rotation);
                }
                else
                {
                    tc.Translation = position;
                    tc.Rotation = glm::eulerAngles(rotation);
                }
            }
        }

        if (!ScriptEngine::IsInitialized())
            return;

        auto& scriptEngine = ScriptEngine::Get();

        const auto view = m_Registry.view<ScriptComponent>();
        for (const std::vector scripted(view.begin(), view.end()); const auto e : scripted)
        {
            if (!m_Registry.valid(e))
                continue;

            Entity entity(e, this);
            scriptEngine.OnUpdateEntity(entity, timestep);
        }

        // Carry out destructions scripts queued this frame, now that the script view
        // is no longer being iterated.
        FlushDestroyQueue();
    }

    auto Scene::OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& camera) -> void
    {
        EP_PROFILE_FN("Scene::OnRenderEditor");

        sceneRenderer->BeginScene(camera);
        RenderScene(sceneRenderer);
        sceneRenderer->EndScene();
    }

    auto Scene::OnRenderRuntime(const Ref<SceneRenderer>& sceneRenderer) -> void
    {
        EP_PROFILE_FN("Scene::OnRenderRuntime");

        const Entity cameraEntity = GetPrimaryCameraEntity();
        if (!cameraEntity)
            return; // No camera to render through (already warned on runtime start).

        const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        const auto& transform = cameraEntity.GetComponent<TransformComponent>();

        sceneRenderer->BeginScene(camera, transform.GetTransform());
        RenderScene(sceneRenderer);
        sceneRenderer->EndScene();
    }

    auto Scene::GetPrimaryCameraEntity() -> Entity
    {
        for (const auto view = m_Registry.view<CameraComponent>(); const auto e : view)
        {
            if (view.get<CameraComponent>(e).Primary)
                return { e, this };
        }

        return {};
    }

    auto Scene::CreateEntity(const std::string& name) -> Entity
    {
        return CreateEntityWithUUID(UUID(), name);
    }

    auto Scene::CreateEntityWithUUID(const UUID& uuid, const std::string& name) -> Entity
    {
        Entity entity(m_Registry.create(), this);

        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        m_EntityMap[uuid] = entity;

        return entity;
    }

    auto Scene::GetEntityByUUID(const UUID& uuid) -> Entity
    {
        if (const auto it = m_EntityMap.find(uuid); it != m_EntityMap.end())
            return { it->second, this };

        return {};
    }

    auto Scene::FindEntityByName(const std::string& name) -> Entity
    {
        for (const auto view = m_Registry.view<TagComponent>(); const auto e : view)
        {
            if (view.get<TagComponent>(e).Tag == name)
                return { e, this };
        }

        return {};
    }

    auto Scene::DuplicateEntity(const Entity entity) -> Entity
    {
        EP_PROFILE_FN("Scene::DuplicateEntity");

        const Entity parent =
            entity.HasComponent<RelationshipComponent>() ? GetEntityByUUID(entity.GetComponent<RelationshipComponent>().Parent) : Entity{};
        std::function<Entity(Entity, Entity)> duplicateHierarchy = [&](const Entity source, Entity newParent) -> Entity
        {
            Entity duplicate = CreateEntity(source.GetName());
            const bool hasRelationship = source.HasComponent<RelationshipComponent>();
            if (hasRelationship)
                duplicate.AddComponent<RelationshipComponent>();

            TryCopyComponent<TransformComponent>(source, duplicate);
            TryCopyComponent<MeshComponent>(source, duplicate);
            TryCopyComponent<CameraComponent>(source, duplicate);
            TryCopyComponent<DirectionalLightComponent>(source, duplicate);
            TryCopyComponent<PointLightComponent>(source, duplicate);
            TryCopyComponent<ScriptComponent>(source, duplicate);
            TryCopyComponent<RigidBodyComponent>(source, duplicate);
            TryCopyComponent<BoxColliderComponent>(source, duplicate);
            TryCopyComponent<SphereColliderComponent>(source, duplicate);
            TryCopyComponent<CapsuleColliderComponent>(source, duplicate);
            TryCopyComponent<CylinderColliderComponent>(source, duplicate);

            if (source.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
                ScriptEngine::Get().CopyFieldMap(source.GetUUID(), duplicate.GetUUID());

            if (newParent)
            {
                duplicate.TryAddComponent<RelationshipComponent>().Parent = newParent.GetUUID();
                newParent.TryAddComponent<RelationshipComponent>().Children.push_back(duplicate.GetUUID());
            }

            const std::vector<UUID> children =
                hasRelationship ? source.GetComponent<RelationshipComponent>().Children : std::vector<UUID>{};
            for (const UUID childId : children)
            {
                if (const Entity child = GetEntityByUUID(childId))
                    duplicateHierarchy(child, duplicate);
            }

            return duplicate;
        };

        return duplicateHierarchy(entity, parent);
    }

    auto Scene::DestroyEntity(const Entity entity) -> void
    {
        // Detach from the parent so its child list stays valid, then destroy the subtree.
        if (const UUID parentId =
                entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Parent : UUID(0))
        {
            if (const Entity parent = GetEntityByUUID(parentId))
            {
                if (parent.HasComponent<RelationshipComponent>())
                {
                    auto& relationship = parent.GetComponent<RelationshipComponent>();
                    std::erase(relationship.Children, entity.GetUUID());
                    if (!relationship.Parent && relationship.Children.empty())
                        parent.RemoveComponent<RelationshipComponent>();
                }
            }
        }

        DestroyEntityHierarchy(entity);
    }

    auto Scene::DestroyEntityDeferred(const Entity entity) -> void
    {
        if (!entity)
            return;

        m_EntitiesToDestroy.push_back(entity.GetUUID());
    }

    auto Scene::FitColliderToMesh(const Entity entity, BoxColliderComponent& collider) -> void
    {
        AABB bounds;
        if (!TryGetMeshBounds(entity, bounds))
            return;

        collider.Offset = bounds.GetCenter();
        collider.HalfSize = bounds.GetHalfExtent();
    }

    auto Scene::FitColliderToMesh(const Entity entity, SphereColliderComponent& collider) -> void
    {
        AABB bounds;
        if (!TryGetMeshBounds(entity, bounds))
            return;

        const glm::vec3 halfExtent = bounds.GetHalfExtent();
        collider.Offset = bounds.GetCenter();
        collider.Radius = std::max({ halfExtent.x, halfExtent.y, halfExtent.z });
    }

    auto Scene::FitColliderToMesh(const Entity entity, CapsuleColliderComponent& collider) -> void
    {
        AABB bounds;
        if (!TryGetMeshBounds(entity, bounds))
            return;

        const glm::vec3 halfExtent = bounds.GetHalfExtent();
        collider.Offset = bounds.GetCenter();
        collider.Radius = std::max(halfExtent.x, halfExtent.z);
        collider.Height = std::max(0.0f, halfExtent.y * 2.0f - collider.Radius * 2.0f);
    }

    auto Scene::FitColliderToMesh(const Entity entity, CylinderColliderComponent& collider) -> void
    {
        AABB bounds;
        if (!TryGetMeshBounds(entity, bounds))
            return;

        const glm::vec3 halfExtent = bounds.GetHalfExtent();
        collider.Offset = bounds.GetCenter();
        collider.Radius = std::max(halfExtent.x, halfExtent.z);
        collider.Height = halfExtent.y * 2.0f;
    }

    auto Scene::DestroyEntityHierarchy(const Entity entity) -> void
    {
        // Snapshot the child list: recursing destroys entities and frees the component.
        const std::vector<UUID> children =
            entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Children : std::vector<UUID>{};
        for (const UUID childId : children)
        {
            if (const Entity child = GetEntityByUUID(childId))
                DestroyEntityHierarchy(child);
        }

        if (entity.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
        {
            auto& scriptEngine = ScriptEngine::Get();
            scriptEngine.OnDestroyEntity(entity); // run managed OnDestroy, unregister the live instance
            scriptEngine.RemoveFieldMap(entity.GetUUID());
        }

        m_EntityMap.erase(entity.GetUUID());
        m_Registry.destroy(entity);
    }

    auto Scene::FlushDestroyQueue() -> void
    {
        if (m_EntitiesToDestroy.empty())
            return;

        // Move out first so a destroy can't re-enter the queue mid-drain; a queued
        // descendant already taken by an ancestor's subtree simply won't resolve.
        const std::vector<UUID> pending = std::move(m_EntitiesToDestroy);
        m_EntitiesToDestroy.clear();
        for (const UUID id : pending)
        {
            if (const Entity entity = GetEntityByUUID(id))
                DestroyEntity(entity);
        }
    }

    auto Scene::SetParent(Entity child, Entity parent) -> void
    {
        EP_PROFILE_FN("Scene::SetParent");
        if (!child)
            return;

        // Reject cycles: walking up from the new parent must not reach the child.
        for (Entity ancestor = parent; ancestor;)
        {
            if (ancestor == child)
            {
                Log::Warn("Ignoring reparent of '{}': target is itself or a descendant.", child.GetName());
                return;
            }

            const UUID ancestorParent =
                ancestor.HasComponent<RelationshipComponent>() ? ancestor.GetComponent<RelationshipComponent>().Parent : UUID(0);
            ancestor = ancestorParent ? GetEntityByUUID(ancestorParent) : Entity{};
        }

        // Capture the world transform to preserve it across the parent change.
        const glm::mat4 worldTransform = GetWorldTransform(child);

        // Unlink from the current parent, if any.
        if (const UUID oldParentId =
                child.HasComponent<RelationshipComponent>() ? child.GetComponent<RelationshipComponent>().Parent : UUID(0))
        {
            if (const Entity oldParent = GetEntityByUUID(oldParentId); oldParent && oldParent.HasComponent<RelationshipComponent>())
            {
                auto& oldRelationship = oldParent.GetComponent<RelationshipComponent>();
                std::erase(oldRelationship.Children, child.GetUUID());
                if (!oldRelationship.Parent && oldRelationship.Children.empty())
                    oldParent.RemoveComponent<RelationshipComponent>();
            }
        }

        // Link to the new parent (or become a root when parent is invalid).
        if (parent)
        {
            child.TryAddComponent<RelationshipComponent>().Parent = parent.GetUUID();
            auto& children = parent.TryAddComponent<RelationshipComponent>().Children;
            if (std::find(children.begin(), children.end(), child.GetUUID()) == children.end())
                children.push_back(child.GetUUID());
        }
        else if (child.HasComponent<RelationshipComponent>())
        {
            auto& relationship = child.GetComponent<RelationshipComponent>();
            relationship.Parent = 0;
            if (relationship.Children.empty())
                child.RemoveComponent<RelationshipComponent>();
        }

        // Re-solve the local transform so the child stays put in world space.
        const glm::mat4 parentWorld = parent ? GetWorldTransform(parent) : glm::mat4(1.0f);
        const glm::mat4 localTransform = glm::inverse(parentWorld) * worldTransform;

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        auto& transform = child.GetComponent<TransformComponent>();
        glm::decompose(localTransform, transform.Scale, orientation, transform.Translation, skew, perspective);
        transform.Rotation = glm::eulerAngles(orientation);
    }

    auto Scene::GetWorldTransform(const Entity entity) -> glm::mat4
    {
        glm::mat4 world(1.0f);

        // Walk child -> root, pre-multiplying each local transform. The guard defends
        // against a cycle in malformed serialized data.
        Entity current = entity;
        for (size_t guard = 0; current; ++guard)
        {
            world = current.GetComponent<TransformComponent>().GetTransform() * world;

            const UUID parentId =
                current.HasComponent<RelationshipComponent>() ? current.GetComponent<RelationshipComponent>().Parent : UUID(0);
            if (!parentId)
                break;

            if (guard > m_EntityMap.size())
            {
                Log::Error("Cycle detected while composing world transform for entity '{}'; aborting walk.", entity.GetName());
                break;
            }

            current = GetEntityByUUID(parentId);
        }

        return world;
    }

    auto Scene::GetWorldRotation(const Entity entity) -> glm::quat
    {
        glm::quat world(1.0f, 0.0f, 0.0f, 0.0f);

        Entity current = entity;
        for (size_t guard = 0; current; ++guard)
        {
            world = glm::quat(current.GetComponent<TransformComponent>().Rotation) * world;

            const UUID parentId =
                current.HasComponent<RelationshipComponent>() ? current.GetComponent<RelationshipComponent>().Parent : UUID(0);
            if (!parentId)
                break;

            if (guard > m_EntityMap.size())
            {
                Log::Error("Cycle detected while composing world rotation for entity '{}'; aborting walk.", entity.GetName());
                break;
            }

            current = GetEntityByUUID(parentId);
        }

        return world;
    }

    auto Scene::ForEachEntity(const std::function<void(Entity)>& func) -> void
    {
        const auto view = m_Registry.view<IDComponent>();
        for (const std::vector entities(view.begin(), view.end()); const EntityHandle e : entities)
        {
            if (!m_Registry.valid(e))
                continue;

            func(Entity{ e, this });
        }
    }

    auto Scene::SortEntitiesByID() -> void
    {
        m_Registry.sort<IDComponent>(
            [](const IDComponent& lhs, const IDComponent& rhs)
            {
                return lhs.ID < rhs.ID;
            }
        );
    }

    template<typename T>
    auto Scene::TryCopyComponent(const Entity srcEntity, Entity dstEntity) -> void
    {
        EP_PROFILE_FN("Scene::TryCopyComponent");

        if (srcEntity.HasComponent<T>())
            dstEntity.AddOrReplaceComponent<T>(srcEntity.GetComponent<T>());
    }

    template<typename T>
    auto
    Scene::CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, EntityHandle>& entityMap)
        -> void
    {
        EP_PROFILE_FN("Scene::CopyComponent");

        for (auto view = srcRegistry.view<T>(); auto srcEntity : view)
        {
            EntityHandle dstEntity = entityMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);
            auto& srcComponent = srcRegistry.get<T>(srcEntity);
            dstRegistry.emplace_or_replace<T>(dstEntity, srcComponent);
        }
    }

    auto Scene::Copy(const Ref<Scene>& scene) -> Ref<Scene>
    {
        EP_PROFILE_FN("Scene::Copy");

        Ref<Scene> newScene = CreateRef<Scene>();

        auto& srcRegistry = scene->m_Registry;
        auto& dstRegistry = newScene->m_Registry;

        std::unordered_map<UUID, EntityHandle> entityMap;
        const auto idView = srcRegistry.view<IDComponent>();

        // entt views iterate newest-first (reverse creation order). Recreating in
        // that order would flip the destination registry's creation order relative
        // to the source, so the hierarchy panel (which also walks newest-first)
        // would show a reversed list during play. Walk the source in reverse so the
        // copy preserves the original creation order.
        const std::vector srcEntities(idView.begin(), idView.end());
        for (auto it = srcEntities.rbegin(); it != srcEntities.rend(); ++it)
        {
            const auto entity = *it;
            auto uuid = srcRegistry.get<IDComponent>(entity).ID;
            const auto& name = srcRegistry.get<TagComponent>(entity).Tag;
            const Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
            entityMap[uuid] = newEntity;
        }

        CopyComponent<TransformComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<MeshComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<CameraComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<DirectionalLightComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<PointLightComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<ScriptComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<RigidBodyComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<BoxColliderComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<SphereColliderComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<CapsuleColliderComponent>(srcRegistry, dstRegistry, entityMap);
        CopyComponent<CylinderColliderComponent>(srcRegistry, dstRegistry, entityMap);
        // UUID-based links copy verbatim, no handle remapping needed.
        CopyComponent<RelationshipComponent>(srcRegistry, dstRegistry, entityMap);

        newScene->m_EnvironmentSettings = scene->m_EnvironmentSettings;
        newScene->m_BloomSettings = scene->m_BloomSettings;
        newScene->m_SsaoSettings = scene->m_SsaoSettings;

        return newScene;
    }

    auto Scene::RenderScene(const Ref<SceneRenderer>& sceneRenderer) -> void
    {
        EP_PROFILE_FN("Scene::RenderScene");

        sceneRenderer->SubmitEnvironmentSettings(m_EnvironmentSettings);
        sceneRenderer->SubmitBloomSettings(m_BloomSettings);
        sceneRenderer->SubmitSsaoSettings(m_SsaoSettings);

        for (const auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>(); const auto& entity : view)
        {
            const auto& lightComponent = view.get<DirectionalLightComponent>(entity);
            const glm::vec3 direction = GetWorldRotation(Entity(entity, this)) * glm::vec3(0.0f, -1.0f, 0.0f);
            sceneRenderer->SubmitDirectionalLight(direction, lightComponent.Color, lightComponent.Intensity);
        }

        for (const auto view = m_Registry.view<PointLightComponent, TransformComponent>(); const auto& entity : view)
        {
            const auto& lightComponent = view.get<PointLightComponent>(entity);
            const auto worldPosition = glm::vec3(GetWorldTransform(Entity(entity, this))[3]);
            sceneRenderer->SubmitPointLight(worldPosition, lightComponent.Color, lightComponent.Intensity, lightComponent.Range);
        }

        for (const auto view = m_Registry.view<MeshComponent, TransformComponent>(); const auto& entity : view)
        {
            if (const auto& meshComponent = view.get<MeshComponent>(entity); meshComponent.MeshHandle)
            {
                sceneRenderer->SubmitMesh(meshComponent.MeshHandle, GetWorldTransform(Entity(entity, this)));
            }
        }
    }
}
