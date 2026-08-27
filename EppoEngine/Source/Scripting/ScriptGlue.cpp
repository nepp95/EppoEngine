#include "pch.h"
#include "Scripting/ScriptGlue.h"

#include "Asset/AssetManager.h"
#include "Core/Input.h"
#include "Physics/PhysicsWorld.h"
#include "Project/Project.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scripting/ScriptEngine.h"

namespace Eppo
{
    namespace
    {
        // Resolve a script-side entity id (its UUID) back to a live Entity via the
        // scene set on play. Returns a null Entity when no scene is active or the
        // id is unknown; callers must check it.
        auto GetEntity(const uint64_t id) -> Entity
        {
            if (!ScriptEngine::IsInitialized())
                return {};

            auto scene = ScriptEngine::Get().GetSceneContext();
            if (!scene)
            {
                Log::Error("Script internal call made with no active scene");
                return {};
            }

            return scene->GetEntityByUUID(UUID(id));
        }

        auto GetScene() -> Ref<Scene>
        {
            if (!ScriptEngine::IsInitialized())
                return {};

            auto scene = ScriptEngine::Get().GetSceneContext();
            if (!scene)
            {
                Log::Error("Script internal call made with no active scene");
                return nullptr;
            }

            return scene;
        }

        // Called from managed via C# `delegate* unmanaged[Cdecl]`. The engine is
        // x64-only (single calling convention), so a plain function matches.
        auto LogMessage(const uint8_t level, const char* message) -> void
        {
            EP_ASSERT(level <= 3);
            switch (level)
            {
                case 0:
                {
                    Log::Trace(LogSource::Script, "{}", message);
                    break;
                }

                case 1:
                {
                    Log::Info(LogSource::Script, "{}", message);
                    break;
                }

                case 2:
                {
                    Log::Warn(LogSource::Script, "{}", message);
                    break;
                }

                case 3:
                default:
                {
                    Log::Error(LogSource::Script, "{}", message);
                    break;
                }
            }
        }

#pragma region Core
        auto Input_IsKeyPressed(const uint16_t keyCode) -> uint8_t
        {
            return Input::IsKeyPressed(keyCode);
        }

        auto Input_IsMouseButtonPressed(const uint16_t button) -> uint8_t
        {
            return Input::IsMouseButtonPressed(button);
        }

        auto Input_GetMousePosition(glm::vec2* outPosition) -> void
        {
            *outPosition = Input::GetMousePosition();
        }
#pragma endregion

#pragma region Physics
        auto Physics_ApplyLinearImpulse(const uint64_t id, const glm::vec3* impulse) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity)
                return;

            auto world = ScriptEngine::Get().GetActivePhysicsWorld();
            if (!world)
                return;

            world->ApplyLinearImpulse(entity.GetUUID(), *impulse);
        }

        auto Physics_GetLinearVelocity(const uint64_t id, glm::vec3* outVelocity) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity)
                return;

            auto world = ScriptEngine::Get().GetActivePhysicsWorld();
            if (!world)
                return;

            *outVelocity = world->GetLinearVelocity(entity.GetUUID());
        }

        auto Physics_SetLinearVelocity(const uint64_t id, const glm::vec3* velocity) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity)
                return;

            auto world = ScriptEngine::Get().GetActivePhysicsWorld();
            if (!world)
                return;

            world->SetLinearVelocity(entity.GetUUID(), *velocity);
        }

        // Blittable ray-cast result marshalled to managed. Layout must match the
        // RaycastHit struct in InternalCalls.cs.
        struct ScriptRayHit
        {
            glm::vec3 Point;
            glm::vec3 Normal;
            uint64_t EntityId;
            float Distance;
            uint8_t Hit;
        };

        auto Physics_Raycast(const glm::vec3* origin, const glm::vec3* direction, const float maxDistance, ScriptRayHit* outHit) -> void
        {
            if (!outHit)
                return;

            outHit->Hit = 0;
            outHit->EntityId = 0;
            outHit->Distance = 0.0f;
            outHit->Point = glm::vec3(0.0f);
            outHit->Normal = glm::vec3(0.0f);

            const auto world = ScriptEngine::Get().GetActivePhysicsWorld();
            if (!world)
                return;

            const RayHit hit = world->CastRay(*origin, *direction, maxDistance);
            if (!hit.Hit)
                return;

            outHit->Hit = 1;
            outHit->Point = hit.Point;
            outHit->Normal = hit.Normal;
            outHit->Distance = hit.Distance;
            outHit->EntityId = static_cast<uint64_t>(hit.EntityId);
        }

        auto Physics_OverlapsSphere(const uint64_t id, const glm::vec3* center, const float radius) -> uint8_t
        {
            const auto world = ScriptEngine::Get().GetActivePhysicsWorld();
            if (!world)
                return false;

            return world->OverlapsSphere(UUID(id), *center, radius);
        }
#pragma endregion

#pragma region Scene
        auto Entity_HasComponent(const uint64_t id, const char* typeName) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity)
                return false;

            const std::string_view name(typeName);
            if (name == "TransformComponent")
                return entity.HasComponent<TransformComponent>();
            if (name == "MeshComponent")
                return entity.HasComponent<MeshComponent>();
            if (name == "CameraComponent")
                return entity.HasComponent<CameraComponent>();
            if (name == "DirectionalLightComponent")
                return entity.HasComponent<DirectionalLightComponent>();
            if (name == "PointLightComponent")
                return entity.HasComponent<PointLightComponent>();
            if (name == "ScriptComponent")
                return entity.HasComponent<ScriptComponent>();
            if (name == "RelationshipComponent")
                return entity.HasComponent<RelationshipComponent>();
            if (name == "RigidBodyComponent")
                return entity.HasComponent<RigidBodyComponent>();
            if (name == "BoxColliderComponent")
                return entity.HasComponent<BoxColliderComponent>();
            if (name == "SphereColliderComponent")
                return entity.HasComponent<SphereColliderComponent>();
            if (name == "CapsuleColliderComponent")
                return entity.HasComponent<CapsuleColliderComponent>();
            if (name == "CylinderColliderComponent")
                return entity.HasComponent<CylinderColliderComponent>();

            return false;
        }

        auto Entity_AddComponent(const uint64_t id, const char* typeName) -> void
        {
            Entity entity = GetEntity(id);
            if (!entity)
                return;

            const std::string_view name(typeName);
            if (name == "TransformComponent")
                entity.TryAddComponent<TransformComponent>();
            if (name == "MeshComponent")
                entity.TryAddComponent<MeshComponent>();
            if (name == "CameraComponent")
                entity.TryAddComponent<CameraComponent>();
            if (name == "DirectionalLightComponent")
                entity.TryAddComponent<DirectionalLightComponent>();
            if (name == "PointLightComponent")
                entity.TryAddComponent<PointLightComponent>();
            if (name == "ScriptComponent")
                entity.TryAddComponent<ScriptComponent>();
            if (name == "RelationshipComponent")
                entity.TryAddComponent<RelationshipComponent>();
            if (name == "RigidBodyComponent")
                entity.TryAddComponent<RigidBodyComponent>();
            if (name == "BoxColliderComponent")
                entity.TryAddComponent<BoxColliderComponent>();
            if (name == "SphereColliderComponent")
                entity.TryAddComponent<SphereColliderComponent>();
            if (name == "CapsuleColliderComponent")
                entity.TryAddComponent<CapsuleColliderComponent>();
            if (name == "CylinderColliderComponent")
                entity.TryAddComponent<CylinderColliderComponent>();
        }

        auto Entity_RemoveComponent(const uint64_t id, const char* typeName) -> uint8_t
        {
            Entity entity = GetEntity(id);
            if (!entity)
                return false;

            const std::string_view name(typeName);
            if (name == "TransformComponent")
                return entity.RemoveComponent<TransformComponent>();
            if (name == "MeshComponent")
                return entity.RemoveComponent<MeshComponent>();
            if (name == "CameraComponent")
                return entity.RemoveComponent<CameraComponent>();
            if (name == "DirectionalLightComponent")
                return entity.RemoveComponent<DirectionalLightComponent>();
            if (name == "PointLightComponent")
                return entity.RemoveComponent<PointLightComponent>();
            if (name == "ScriptComponent")
                return entity.RemoveComponent<ScriptComponent>();
            if (name == "RelationshipComponent")
                return entity.RemoveComponent<RelationshipComponent>();
            if (name == "RigidBodyComponent")
                return entity.RemoveComponent<RigidBodyComponent>();
            if (name == "BoxColliderComponent")
                return entity.RemoveComponent<BoxColliderComponent>();
            if (name == "SphereColliderComponent")
                return entity.RemoveComponent<SphereColliderComponent>();
            if (name == "CapsuleColliderComponent")
                return entity.RemoveComponent<CapsuleColliderComponent>();
            if (name == "CylinderColliderComponent")
                return entity.RemoveComponent<CylinderColliderComponent>();

            return false;
        }

        // Returns the entity's name (its TagComponent tag) to managed. The pointer
        // is into the live component string; the managed caller copies it into a
        // string immediately (synchronous call), so no ownership crosses the boundary.
        auto Entity_GetName(const uint64_t id) -> const char*
        {
            const Entity entity = GetEntity(id);
            if (!entity)
                return "";

            return entity.GetName().c_str();
        }

        auto Entity_SetName(const uint64_t id, const char* name) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TagComponent>())
                return;

            entity.GetComponent<TagComponent>().Tag = name ? std::string(name) : std::string();
        }

        auto Scene_CreateEntity(const char* name) -> uint64_t
        {
            const auto& scene = GetScene();
            if (!scene)
                return 0;

            Entity entity = scene->CreateEntity(name ? std::string(name) : std::string());
            return static_cast<uint64_t>(entity.GetUUID());
        }

        auto Scene_DestroyEntity(const uint64_t id) -> void
        {
            const auto& scene = GetScene();
            if (!scene)
                return;

            const Entity entity = scene->GetEntityByUUID(UUID(id));
            if (!entity)
                return;

            scene->DestroyEntityDeferred(entity);
        }

        auto Scene_FindEntityByName(const char* name) -> uint64_t
        {
            const auto& scene = GetScene();
            if (!scene || !name)
                return 0;

            const Entity entity = scene->FindEntityByName(std::string(name));
            return entity ? static_cast<uint64_t>(entity.GetUUID()) : 0;
        }

        auto TransformComponent_GetTranslation(const uint64_t id, glm::vec3* outTranslation) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            *outTranslation = entity.GetComponent<TransformComponent>().Translation;
        }

        auto TransformComponent_SetTranslation(const uint64_t id, const glm::vec3* translation) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            entity.GetComponent<TransformComponent>().Translation = *translation;
        }

        auto TransformComponent_GetRotation(const uint64_t id, glm::vec3* outRotation) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            *outRotation = entity.GetComponent<TransformComponent>().Rotation;
        }

        auto TransformComponent_SetRotation(const uint64_t id, const glm::vec3* rotation) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            entity.GetComponent<TransformComponent>().Rotation = *rotation;
        }

        auto TransformComponent_GetScale(const uint64_t id, glm::vec3* outScale) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            *outScale = entity.GetComponent<TransformComponent>().Scale;
        }

        auto TransformComponent_SetScale(const uint64_t id, const glm::vec3* scale) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<TransformComponent>())
                return;

            entity.GetComponent<TransformComponent>().Scale = *scale;
        }

        auto MeshComponent_GetMeshHandle(const uint64_t id) -> uint64_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<MeshComponent>())
                return 0;

            return static_cast<uint64_t>(entity.GetComponent<MeshComponent>().MeshHandle);
        }

        // A bad handle would reach the renderer as a silently invisible entity.
        auto MeshComponent_SetMeshHandle(const uint64_t id, const uint64_t meshHandle) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<MeshComponent>())
                return;

            const AssetHandle handle(meshHandle);

            // 0 clears the assignment; primitives are generated on demand and need no
            // registry entry.
            const bool isPrimitive = meshHandle >= static_cast<uint64_t>(MeshPrimitiveType::Cone) &&
                meshHandle <= static_cast<uint64_t>(MeshPrimitiveType::Capsule);

            if (meshHandle != 0 && !isPrimitive)
            {
                const auto& project = Project::GetActive();
                if (!project || !project->GetAssetManager())
                {
                    Log::Error(LogSource::Script, "Cannot assign mesh handle {}: no active project", meshHandle);
                    return;
                }

                const auto& assetManager = project->GetAssetManager();
                if (!assetManager->HasAssetData(handle))
                {
                    Log::Error(LogSource::Script, "Cannot assign mesh handle {}: no such asset", meshHandle);
                    return;
                }

                if (assetManager->GetMetadata(handle).Type != AssetType::Mesh)
                {
                    Log::Error(LogSource::Script, "Cannot assign mesh handle {}: asset is not a mesh", meshHandle);
                    return;
                }
            }

            entity.GetComponent<MeshComponent>().MeshHandle = handle;
        }

        auto CameraComponent_GetPrimary(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return false;

            return entity.GetComponent<CameraComponent>().Primary;
        }

        auto CameraComponent_SetPrimary(const uint64_t id, const uint8_t primary) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return;

            entity.GetComponent<CameraComponent>().Primary = primary != 0;
        }

        auto CameraComponent_GetVerticalFov(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return 0.0f;

            return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveVerticalFov();
        }

        auto CameraComponent_SetVerticalFov(const uint64_t id, const float verticalFov) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return;

            entity.GetComponent<CameraComponent>().Camera.SetPerspectiveVerticalFov(verticalFov);
        }

        auto CameraComponent_GetNearClip(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return 0.0f;

            return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveNearClip();
        }

        auto CameraComponent_SetNearClip(const uint64_t id, const float nearClip) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return;

            entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(nearClip);
        }

        auto CameraComponent_GetFarClip(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return 0.0f;

            return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveFarClip();
        }

        auto CameraComponent_SetFarClip(const uint64_t id, const float farClip) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CameraComponent>())
                return;

            entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(farClip);
        }

        auto DirectionalLightComponent_GetColor(const uint64_t id, glm::vec3* outColor) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<DirectionalLightComponent>())
                return;

            *outColor = entity.GetComponent<DirectionalLightComponent>().Color;
        }

        auto DirectionalLightComponent_SetColor(const uint64_t id, const glm::vec3* color) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<DirectionalLightComponent>())
                return;

            entity.GetComponent<DirectionalLightComponent>().Color = *color;
        }

        auto DirectionalLightComponent_GetIntensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<DirectionalLightComponent>())
                return 0.0f;

            return entity.GetComponent<DirectionalLightComponent>().Intensity;
        }

        auto DirectionalLightComponent_SetIntensity(const uint64_t id, const float intensity) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<DirectionalLightComponent>())
                return;

            entity.GetComponent<DirectionalLightComponent>().Intensity = intensity;
        }

        auto PointLightComponent_GetColor(const uint64_t id, glm::vec3* outColor) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            *outColor = entity.GetComponent<PointLightComponent>().Color;
        }

        auto PointLightComponent_SetColor(const uint64_t id, const glm::vec3* color) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            entity.GetComponent<PointLightComponent>().Color = *color;
        }

        auto PointLightComponent_GetIntensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return 0.0f;

            return entity.GetComponent<PointLightComponent>().Intensity;
        }

        auto PointLightComponent_SetIntensity(const uint64_t id, const float intensity) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            entity.GetComponent<PointLightComponent>().Intensity = intensity;
        }

        auto PointLightComponent_GetRange(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return 0.0f;

            return entity.GetComponent<PointLightComponent>().Range;
        }

        auto PointLightComponent_SetRange(const uint64_t id, const float range) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<PointLightComponent>())
                return;

            entity.GetComponent<PointLightComponent>().Range = range;
        }

        auto RelationshipComponent_GetParent(const uint64_t id) -> uint64_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RelationshipComponent>())
                return 0;

            return static_cast<uint64_t>(entity.GetComponent<RelationshipComponent>().Parent);
        }

        auto RelationshipComponent_SetParent(const uint64_t id, const uint64_t parent) -> void
        {
            const auto& scene = GetScene();
            if (!scene)
                return;

            const Entity entity = scene->GetEntityByUUID(UUID(id));
            if (!entity)
                return;

            scene->SetParent(entity, scene->GetEntityByUUID(UUID(parent)));
        }

        auto RelationshipComponent_GetChildCount(const uint64_t id) -> int32_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RelationshipComponent>())
                return 0;

            return static_cast<int32_t>(entity.GetComponent<RelationshipComponent>().Children.size());
        }

        auto RelationshipComponent_GetChild(const uint64_t id, const int32_t index) -> uint64_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RelationshipComponent>())
                return 0;

            const auto& children = entity.GetComponent<RelationshipComponent>().Children;
            if (index < 0 || index >= static_cast<int32_t>(children.size()))
                return 0;

            return static_cast<uint64_t>(children[index]);
        }

        auto RigidBodyComponent_GetType(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return 0;

            return static_cast<uint8_t>(entity.GetComponent<RigidBodyComponent>().Type);
        }

        auto RigidBodyComponent_SetType(const uint64_t id, const uint8_t type) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().Type = static_cast<RigidBodyComponent::BodyType>(type);
        }

        auto RigidBodyComponent_GetGravityScale(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return 0.0f;

            return entity.GetComponent<RigidBodyComponent>().GravityScale;
        }

        auto RigidBodyComponent_SetGravityScale(const uint64_t id, const float gravityScale) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().GravityScale = gravityScale;
        }

        auto RigidBodyComponent_GetLinearDamping(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return 0.0f;

            return entity.GetComponent<RigidBodyComponent>().LinearDamping;
        }

        auto RigidBodyComponent_SetLinearDamping(const uint64_t id, const float linearDamping) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LinearDamping = linearDamping;
        }

        auto RigidBodyComponent_GetAngularDamping(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return 0.0f;

            return entity.GetComponent<RigidBodyComponent>().AngularDamping;
        }

        auto RigidBodyComponent_SetAngularDamping(const uint64_t id, const float angularDamping) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().AngularDamping = angularDamping;
        }

        auto RigidBodyComponent_GetLockLinearX(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return false;

            return entity.GetComponent<RigidBodyComponent>().LockLinearX;
        }

        auto RigidBodyComponent_SetLockLinearX(const uint64_t id, const uint8_t locked) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LockLinearX = locked != 0;
        }

        auto RigidBodyComponent_GetLockLinearY(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return false;

            return entity.GetComponent<RigidBodyComponent>().LockLinearY;
        }

        auto RigidBodyComponent_SetLockLinearY(const uint64_t id, const uint8_t locked) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LockLinearY = locked != 0;
        }

        auto RigidBodyComponent_GetLockLinearZ(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return false;

            return entity.GetComponent<RigidBodyComponent>().LockLinearZ;
        }

        auto RigidBodyComponent_SetLockLinearZ(const uint64_t id, const uint8_t locked) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LockLinearZ = locked != 0;
        }

        auto RigidBodyComponent_GetLockAngularX(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return false;

            return entity.GetComponent<RigidBodyComponent>().LockAngularX;
        }

        auto RigidBodyComponent_SetLockAngularX(const uint64_t id, const uint8_t locked) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LockAngularX = locked != 0;
        }

        auto RigidBodyComponent_GetLockAngularY(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return false;

            return entity.GetComponent<RigidBodyComponent>().LockAngularY;
        }

        auto RigidBodyComponent_SetLockAngularY(const uint64_t id, const uint8_t locked) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LockAngularY = locked != 0;
        }

        auto RigidBodyComponent_GetLockAngularZ(const uint64_t id) -> uint8_t
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return false;

            return entity.GetComponent<RigidBodyComponent>().LockAngularZ;
        }

        auto RigidBodyComponent_SetLockAngularZ(const uint64_t id, const uint8_t locked) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<RigidBodyComponent>())
                return;

            entity.GetComponent<RigidBodyComponent>().LockAngularZ = locked != 0;
        }

        auto BoxColliderComponent_GetHalfSize(const uint64_t id, glm::vec3* outHalfSize) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            *outHalfSize = entity.GetComponent<BoxColliderComponent>().HalfSize;
        }

        auto BoxColliderComponent_SetHalfSize(const uint64_t id, const glm::vec3* halfSize) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            entity.GetComponent<BoxColliderComponent>().HalfSize = *halfSize;
        }

        auto BoxColliderComponent_GetOffset(const uint64_t id, glm::vec3* outOffset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            *outOffset = entity.GetComponent<BoxColliderComponent>().Offset;
        }

        auto BoxColliderComponent_SetOffset(const uint64_t id, const glm::vec3* offset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            entity.GetComponent<BoxColliderComponent>().Offset = *offset;
        }

        auto BoxColliderComponent_GetDensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return 0.0f;

            return entity.GetComponent<BoxColliderComponent>().Density;
        }

        auto BoxColliderComponent_SetDensity(const uint64_t id, const float density) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            entity.GetComponent<BoxColliderComponent>().Density = density;
        }

        auto BoxColliderComponent_GetFriction(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return 0.0f;

            return entity.GetComponent<BoxColliderComponent>().Friction;
        }

        auto BoxColliderComponent_SetFriction(const uint64_t id, const float friction) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            entity.GetComponent<BoxColliderComponent>().Friction = friction;
        }

        auto BoxColliderComponent_GetRestitution(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return 0.0f;

            return entity.GetComponent<BoxColliderComponent>().Restitution;
        }

        auto BoxColliderComponent_SetRestitution(const uint64_t id, const float restitution) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<BoxColliderComponent>())
                return;

            entity.GetComponent<BoxColliderComponent>().Restitution = restitution;
        }

        auto SphereColliderComponent_GetRadius(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return 0.0f;

            return entity.GetComponent<SphereColliderComponent>().Radius;
        }

        auto SphereColliderComponent_SetRadius(const uint64_t id, const float radius) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return;

            entity.GetComponent<SphereColliderComponent>().Radius = radius;
        }

        auto SphereColliderComponent_GetOffset(const uint64_t id, glm::vec3* outOffset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return;

            *outOffset = entity.GetComponent<SphereColliderComponent>().Offset;
        }

        auto SphereColliderComponent_SetOffset(const uint64_t id, const glm::vec3* offset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return;

            entity.GetComponent<SphereColliderComponent>().Offset = *offset;
        }

        auto SphereColliderComponent_GetDensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return 0.0f;

            return entity.GetComponent<SphereColliderComponent>().Density;
        }

        auto SphereColliderComponent_SetDensity(const uint64_t id, const float density) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return;

            entity.GetComponent<SphereColliderComponent>().Density = density;
        }

        auto SphereColliderComponent_GetFriction(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return 0.0f;

            return entity.GetComponent<SphereColliderComponent>().Friction;
        }

        auto SphereColliderComponent_SetFriction(const uint64_t id, const float friction) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return;

            entity.GetComponent<SphereColliderComponent>().Friction = friction;
        }

        auto SphereColliderComponent_GetRestitution(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return 0.0f;

            return entity.GetComponent<SphereColliderComponent>().Restitution;
        }

        auto SphereColliderComponent_SetRestitution(const uint64_t id, const float restitution) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<SphereColliderComponent>())
                return;

            entity.GetComponent<SphereColliderComponent>().Restitution = restitution;
        }

        auto CapsuleColliderComponent_GetRadius(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CapsuleColliderComponent>().Radius;
        }

        auto CapsuleColliderComponent_SetRadius(const uint64_t id, const float radius) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            entity.GetComponent<CapsuleColliderComponent>().Radius = radius;
        }

        auto CapsuleColliderComponent_GetHeight(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CapsuleColliderComponent>().Height;
        }

        auto CapsuleColliderComponent_SetHeight(const uint64_t id, const float height) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            entity.GetComponent<CapsuleColliderComponent>().Height = height;
        }

        auto CapsuleColliderComponent_GetOffset(const uint64_t id, glm::vec3* outOffset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            *outOffset = entity.GetComponent<CapsuleColliderComponent>().Offset;
        }

        auto CapsuleColliderComponent_SetOffset(const uint64_t id, const glm::vec3* offset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            entity.GetComponent<CapsuleColliderComponent>().Offset = *offset;
        }

        auto CapsuleColliderComponent_GetDensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CapsuleColliderComponent>().Density;
        }

        auto CapsuleColliderComponent_SetDensity(const uint64_t id, const float density) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            entity.GetComponent<CapsuleColliderComponent>().Density = density;
        }

        auto CapsuleColliderComponent_GetFriction(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CapsuleColliderComponent>().Friction;
        }

        auto CapsuleColliderComponent_SetFriction(const uint64_t id, const float friction) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            entity.GetComponent<CapsuleColliderComponent>().Friction = friction;
        }

        auto CapsuleColliderComponent_GetRestitution(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CapsuleColliderComponent>().Restitution;
        }

        auto CapsuleColliderComponent_SetRestitution(const uint64_t id, const float restitution) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CapsuleColliderComponent>())
                return;

            entity.GetComponent<CapsuleColliderComponent>().Restitution = restitution;
        }

        auto CylinderColliderComponent_GetRadius(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CylinderColliderComponent>().Radius;
        }

        auto CylinderColliderComponent_SetRadius(const uint64_t id, const float radius) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            entity.GetComponent<CylinderColliderComponent>().Radius = radius;
        }

        auto CylinderColliderComponent_GetHeight(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CylinderColliderComponent>().Height;
        }

        auto CylinderColliderComponent_SetHeight(const uint64_t id, const float height) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            entity.GetComponent<CylinderColliderComponent>().Height = height;
        }

        auto CylinderColliderComponent_GetOffset(const uint64_t id, glm::vec3* outOffset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            *outOffset = entity.GetComponent<CylinderColliderComponent>().Offset;
        }

        auto CylinderColliderComponent_SetOffset(const uint64_t id, const glm::vec3* offset) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            entity.GetComponent<CylinderColliderComponent>().Offset = *offset;
        }

        auto CylinderColliderComponent_GetDensity(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CylinderColliderComponent>().Density;
        }

        auto CylinderColliderComponent_SetDensity(const uint64_t id, const float density) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            entity.GetComponent<CylinderColliderComponent>().Density = density;
        }

        auto CylinderColliderComponent_GetFriction(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CylinderColliderComponent>().Friction;
        }

        auto CylinderColliderComponent_SetFriction(const uint64_t id, const float friction) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            entity.GetComponent<CylinderColliderComponent>().Friction = friction;
        }

        auto CylinderColliderComponent_GetRestitution(const uint64_t id) -> float
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return 0.0f;

            return entity.GetComponent<CylinderColliderComponent>().Restitution;
        }

        auto CylinderColliderComponent_SetRestitution(const uint64_t id, const float restitution) -> void
        {
            const Entity entity = GetEntity(id);
            if (!entity || !entity.HasComponent<CylinderColliderComponent>())
                return;

            entity.GetComponent<CylinderColliderComponent>().Restitution = restitution;
        }
#pragma endregion
    }

    auto ScriptGlue::GetInternalCalls() -> std::span<const InternalCall>
    {
        static const InternalCall calls[] = {
            { "LogMessage",                               reinterpret_cast<void*>(&LogMessage)                               },
            { "Input_IsKeyPressed",                       reinterpret_cast<void*>(&Input_IsKeyPressed)                       },
            { "Input_IsMouseButtonPressed",               reinterpret_cast<void*>(&Input_IsMouseButtonPressed)               },
            { "Input_GetMousePosition",                   reinterpret_cast<void*>(&Input_GetMousePosition)                   },
            { "Physics_ApplyLinearImpulse",               reinterpret_cast<void*>(&Physics_ApplyLinearImpulse)               },
            { "Physics_GetLinearVelocity",                reinterpret_cast<void*>(&Physics_GetLinearVelocity)                },
            { "Physics_SetLinearVelocity",                reinterpret_cast<void*>(&Physics_SetLinearVelocity)                },
            { "Physics_Raycast",                          reinterpret_cast<void*>(&Physics_Raycast)                          },
            { "Physics_OverlapsSphere",                   reinterpret_cast<void*>(&Physics_OverlapsSphere)                   },
            { "Entity_HasComponent",                      reinterpret_cast<void*>(&Entity_HasComponent)                      },
            { "Entity_AddComponent",                      reinterpret_cast<void*>(&Entity_AddComponent)                      },
            { "Entity_RemoveComponent",                   reinterpret_cast<void*>(&Entity_RemoveComponent)                   },
            { "Entity_GetName",                           reinterpret_cast<void*>(&Entity_GetName)                           },
            { "Entity_SetName",                           reinterpret_cast<void*>(&Entity_SetName)                           },
            { "Scene_CreateEntity",                       reinterpret_cast<void*>(&Scene_CreateEntity)                       },
            { "Scene_DestroyEntity",                      reinterpret_cast<void*>(&Scene_DestroyEntity)                      },
            { "Scene_FindEntityByName",                   reinterpret_cast<void*>(&Scene_FindEntityByName)                   },
            { "TransformComponent_GetTranslation",        reinterpret_cast<void*>(&TransformComponent_GetTranslation)        },
            { "TransformComponent_SetTranslation",        reinterpret_cast<void*>(&TransformComponent_SetTranslation)        },
            { "TransformComponent_GetRotation",           reinterpret_cast<void*>(&TransformComponent_GetRotation)           },
            { "TransformComponent_SetRotation",           reinterpret_cast<void*>(&TransformComponent_SetRotation)           },
            { "TransformComponent_GetScale",              reinterpret_cast<void*>(&TransformComponent_GetScale)              },
            { "TransformComponent_SetScale",              reinterpret_cast<void*>(&TransformComponent_SetScale)              },
            { "MeshComponent_GetMeshHandle",              reinterpret_cast<void*>(&MeshComponent_GetMeshHandle)              },
            { "MeshComponent_SetMeshHandle",              reinterpret_cast<void*>(&MeshComponent_SetMeshHandle)              },
            { "CameraComponent_GetPrimary",               reinterpret_cast<void*>(&CameraComponent_GetPrimary)               },
            { "CameraComponent_SetPrimary",               reinterpret_cast<void*>(&CameraComponent_SetPrimary)               },
            { "CameraComponent_GetVerticalFov",           reinterpret_cast<void*>(&CameraComponent_GetVerticalFov)           },
            { "CameraComponent_SetVerticalFov",           reinterpret_cast<void*>(&CameraComponent_SetVerticalFov)           },
            { "CameraComponent_GetNearClip",              reinterpret_cast<void*>(&CameraComponent_GetNearClip)              },
            { "CameraComponent_SetNearClip",              reinterpret_cast<void*>(&CameraComponent_SetNearClip)              },
            { "CameraComponent_GetFarClip",               reinterpret_cast<void*>(&CameraComponent_GetFarClip)               },
            { "CameraComponent_SetFarClip",               reinterpret_cast<void*>(&CameraComponent_SetFarClip)               },
            { "DirectionalLightComponent_GetColor",       reinterpret_cast<void*>(&DirectionalLightComponent_GetColor)       },
            { "DirectionalLightComponent_SetColor",       reinterpret_cast<void*>(&DirectionalLightComponent_SetColor)       },
            { "DirectionalLightComponent_GetIntensity",   reinterpret_cast<void*>(&DirectionalLightComponent_GetIntensity)   },
            { "DirectionalLightComponent_SetIntensity",   reinterpret_cast<void*>(&DirectionalLightComponent_SetIntensity)   },
            { "PointLightComponent_GetColor",             reinterpret_cast<void*>(&PointLightComponent_GetColor)             },
            { "PointLightComponent_SetColor",             reinterpret_cast<void*>(&PointLightComponent_SetColor)             },
            { "PointLightComponent_GetIntensity",         reinterpret_cast<void*>(&PointLightComponent_GetIntensity)         },
            { "PointLightComponent_SetIntensity",         reinterpret_cast<void*>(&PointLightComponent_SetIntensity)         },
            { "PointLightComponent_GetRange",             reinterpret_cast<void*>(&PointLightComponent_GetRange)             },
            { "PointLightComponent_SetRange",             reinterpret_cast<void*>(&PointLightComponent_SetRange)             },
            { "RelationshipComponent_GetParent",          reinterpret_cast<void*>(&RelationshipComponent_GetParent)          },
            { "RelationshipComponent_SetParent",          reinterpret_cast<void*>(&RelationshipComponent_SetParent)          },
            { "RelationshipComponent_GetChildCount",      reinterpret_cast<void*>(&RelationshipComponent_GetChildCount)      },
            { "RelationshipComponent_GetChild",           reinterpret_cast<void*>(&RelationshipComponent_GetChild)           },
            { "RigidBodyComponent_GetType",               reinterpret_cast<void*>(&RigidBodyComponent_GetType)               },
            { "RigidBodyComponent_SetType",               reinterpret_cast<void*>(&RigidBodyComponent_SetType)               },
            { "RigidBodyComponent_GetGravityScale",       reinterpret_cast<void*>(&RigidBodyComponent_GetGravityScale)       },
            { "RigidBodyComponent_SetGravityScale",       reinterpret_cast<void*>(&RigidBodyComponent_SetGravityScale)       },
            { "RigidBodyComponent_GetLinearDamping",      reinterpret_cast<void*>(&RigidBodyComponent_GetLinearDamping)      },
            { "RigidBodyComponent_SetLinearDamping",      reinterpret_cast<void*>(&RigidBodyComponent_SetLinearDamping)      },
            { "RigidBodyComponent_GetAngularDamping",     reinterpret_cast<void*>(&RigidBodyComponent_GetAngularDamping)     },
            { "RigidBodyComponent_SetAngularDamping",     reinterpret_cast<void*>(&RigidBodyComponent_SetAngularDamping)     },
            { "RigidBodyComponent_GetLockLinearX",        reinterpret_cast<void*>(&RigidBodyComponent_GetLockLinearX)        },
            { "RigidBodyComponent_SetLockLinearX",        reinterpret_cast<void*>(&RigidBodyComponent_SetLockLinearX)        },
            { "RigidBodyComponent_GetLockLinearY",        reinterpret_cast<void*>(&RigidBodyComponent_GetLockLinearY)        },
            { "RigidBodyComponent_SetLockLinearY",        reinterpret_cast<void*>(&RigidBodyComponent_SetLockLinearY)        },
            { "RigidBodyComponent_GetLockLinearZ",        reinterpret_cast<void*>(&RigidBodyComponent_GetLockLinearZ)        },
            { "RigidBodyComponent_SetLockLinearZ",        reinterpret_cast<void*>(&RigidBodyComponent_SetLockLinearZ)        },
            { "RigidBodyComponent_GetLockAngularX",       reinterpret_cast<void*>(&RigidBodyComponent_GetLockAngularX)       },
            { "RigidBodyComponent_SetLockAngularX",       reinterpret_cast<void*>(&RigidBodyComponent_SetLockAngularX)       },
            { "RigidBodyComponent_GetLockAngularY",       reinterpret_cast<void*>(&RigidBodyComponent_GetLockAngularY)       },
            { "RigidBodyComponent_SetLockAngularY",       reinterpret_cast<void*>(&RigidBodyComponent_SetLockAngularY)       },
            { "RigidBodyComponent_GetLockAngularZ",       reinterpret_cast<void*>(&RigidBodyComponent_GetLockAngularZ)       },
            { "RigidBodyComponent_SetLockAngularZ",       reinterpret_cast<void*>(&RigidBodyComponent_SetLockAngularZ)       },
            { "BoxColliderComponent_GetHalfSize",         reinterpret_cast<void*>(&BoxColliderComponent_GetHalfSize)         },
            { "BoxColliderComponent_SetHalfSize",         reinterpret_cast<void*>(&BoxColliderComponent_SetHalfSize)         },
            { "BoxColliderComponent_GetOffset",           reinterpret_cast<void*>(&BoxColliderComponent_GetOffset)           },
            { "BoxColliderComponent_SetOffset",           reinterpret_cast<void*>(&BoxColliderComponent_SetOffset)           },
            { "BoxColliderComponent_GetDensity",          reinterpret_cast<void*>(&BoxColliderComponent_GetDensity)          },
            { "BoxColliderComponent_SetDensity",          reinterpret_cast<void*>(&BoxColliderComponent_SetDensity)          },
            { "BoxColliderComponent_GetFriction",         reinterpret_cast<void*>(&BoxColliderComponent_GetFriction)         },
            { "BoxColliderComponent_SetFriction",         reinterpret_cast<void*>(&BoxColliderComponent_SetFriction)         },
            { "BoxColliderComponent_GetRestitution",      reinterpret_cast<void*>(&BoxColliderComponent_GetRestitution)      },
            { "BoxColliderComponent_SetRestitution",      reinterpret_cast<void*>(&BoxColliderComponent_SetRestitution)      },
            { "SphereColliderComponent_GetRadius",        reinterpret_cast<void*>(&SphereColliderComponent_GetRadius)        },
            { "SphereColliderComponent_SetRadius",        reinterpret_cast<void*>(&SphereColliderComponent_SetRadius)        },
            { "SphereColliderComponent_GetOffset",        reinterpret_cast<void*>(&SphereColliderComponent_GetOffset)        },
            { "SphereColliderComponent_SetOffset",        reinterpret_cast<void*>(&SphereColliderComponent_SetOffset)        },
            { "SphereColliderComponent_GetDensity",       reinterpret_cast<void*>(&SphereColliderComponent_GetDensity)       },
            { "SphereColliderComponent_SetDensity",       reinterpret_cast<void*>(&SphereColliderComponent_SetDensity)       },
            { "SphereColliderComponent_GetFriction",      reinterpret_cast<void*>(&SphereColliderComponent_GetFriction)      },
            { "SphereColliderComponent_SetFriction",      reinterpret_cast<void*>(&SphereColliderComponent_SetFriction)      },
            { "SphereColliderComponent_GetRestitution",   reinterpret_cast<void*>(&SphereColliderComponent_GetRestitution)   },
            { "SphereColliderComponent_SetRestitution",   reinterpret_cast<void*>(&SphereColliderComponent_SetRestitution)   },
            { "CapsuleColliderComponent_GetRadius",       reinterpret_cast<void*>(&CapsuleColliderComponent_GetRadius)       },
            { "CapsuleColliderComponent_SetRadius",       reinterpret_cast<void*>(&CapsuleColliderComponent_SetRadius)       },
            { "CapsuleColliderComponent_GetHeight",       reinterpret_cast<void*>(&CapsuleColliderComponent_GetHeight)       },
            { "CapsuleColliderComponent_SetHeight",       reinterpret_cast<void*>(&CapsuleColliderComponent_SetHeight)       },
            { "CapsuleColliderComponent_GetOffset",       reinterpret_cast<void*>(&CapsuleColliderComponent_GetOffset)       },
            { "CapsuleColliderComponent_SetOffset",       reinterpret_cast<void*>(&CapsuleColliderComponent_SetOffset)       },
            { "CapsuleColliderComponent_GetDensity",      reinterpret_cast<void*>(&CapsuleColliderComponent_GetDensity)      },
            { "CapsuleColliderComponent_SetDensity",      reinterpret_cast<void*>(&CapsuleColliderComponent_SetDensity)      },
            { "CapsuleColliderComponent_GetFriction",     reinterpret_cast<void*>(&CapsuleColliderComponent_GetFriction)     },
            { "CapsuleColliderComponent_SetFriction",     reinterpret_cast<void*>(&CapsuleColliderComponent_SetFriction)     },
            { "CapsuleColliderComponent_GetRestitution",  reinterpret_cast<void*>(&CapsuleColliderComponent_GetRestitution)  },
            { "CapsuleColliderComponent_SetRestitution",  reinterpret_cast<void*>(&CapsuleColliderComponent_SetRestitution)  },
            { "CylinderColliderComponent_GetRadius",      reinterpret_cast<void*>(&CylinderColliderComponent_GetRadius)      },
            { "CylinderColliderComponent_SetRadius",      reinterpret_cast<void*>(&CylinderColliderComponent_SetRadius)      },
            { "CylinderColliderComponent_GetHeight",      reinterpret_cast<void*>(&CylinderColliderComponent_GetHeight)      },
            { "CylinderColliderComponent_SetHeight",      reinterpret_cast<void*>(&CylinderColliderComponent_SetHeight)      },
            { "CylinderColliderComponent_GetOffset",      reinterpret_cast<void*>(&CylinderColliderComponent_GetOffset)      },
            { "CylinderColliderComponent_SetOffset",      reinterpret_cast<void*>(&CylinderColliderComponent_SetOffset)      },
            { "CylinderColliderComponent_GetDensity",     reinterpret_cast<void*>(&CylinderColliderComponent_GetDensity)     },
            { "CylinderColliderComponent_SetDensity",     reinterpret_cast<void*>(&CylinderColliderComponent_SetDensity)     },
            { "CylinderColliderComponent_GetFriction",    reinterpret_cast<void*>(&CylinderColliderComponent_GetFriction)    },
            { "CylinderColliderComponent_SetFriction",    reinterpret_cast<void*>(&CylinderColliderComponent_SetFriction)    },
            { "CylinderColliderComponent_GetRestitution", reinterpret_cast<void*>(&CylinderColliderComponent_GetRestitution) },
            { "CylinderColliderComponent_SetRestitution", reinterpret_cast<void*>(&CylinderColliderComponent_SetRestitution) },
        };

        return calls;
    }
}
