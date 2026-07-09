#include "pch.h"
#include "Scene/Scene.h"

#include "Physics/PhysicsWorld.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scripting/ScriptEngine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace Eppo
{
	// Scene-global gravity (m/s^2); per-body scaling via RigidBodyComponent::GravityScale.
	static constexpr glm::vec3 s_DefaultGravity = { 0.0f, -9.81f, 0.0f };

	namespace
	{
		// Collects an entity's collider components into the shape-agnostic list
		// PhysicsWorld consumes. Adding a collider type touches only this function.
		auto GatherColliders(Entity entity) -> std::vector<ColliderData>
		{
			std::vector<ColliderData> colliders;

			if (entity.HasComponent<BoxColliderComponent>())
			{
				const auto& c = entity.GetComponent<BoxColliderComponent>();
				colliders.push_back({ ColliderShape::Box, c.Offset, c.Density, c.Friction, c.Restitution, c.HalfExtents });
			}

			if (entity.HasComponent<SphereColliderComponent>())
			{
				const auto& c = entity.GetComponent<SphereColliderComponent>();
				ColliderData data{ ColliderShape::Sphere, c.Offset, c.Density, c.Friction, c.Restitution };
				data.Radius = c.Radius;
				colliders.push_back(data);
			}

			if (entity.HasComponent<CapsuleColliderComponent>())
			{
				const auto& c = entity.GetComponent<CapsuleColliderComponent>();
				ColliderData data{ ColliderShape::Capsule, c.Offset, c.Density, c.Friction, c.Restitution };
				data.Radius = c.Radius;
				data.Height = c.Height;
				colliders.push_back(data);
			}

			return colliders;
		}
	}

	auto Scene::SetViewportSize(uint32_t width, uint32_t height) -> void
	{
		// Keep every scene camera's projection aspect ratio in sync with the viewport.
		const auto view = m_Registry.view<CameraComponent>();
		for (const auto e : view)
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
		{
			const auto view = m_Registry.view<RigidBodyComponent, TransformComponent>();
			for (const auto e : view)
			{
				Entity entity(e, this);

				std::vector<ColliderData> colliders = GatherColliders(entity);
				if (colliders.empty() && entity.GetComponent<RigidBodyComponent>().Type == RigidBodyComponent::BodyType::Dynamic)
				{
					// A dynamic body with no shapes has zero mass and ignores
					// gravity.  Inject a unit box so it falls through space
					// until the user adds a real collider.
					Log::Info("Entity '{}': no collider component found; using a default 0.5-unit box so gravity acts.", entity.GetName());
					colliders.push_back(ColliderData{});
				}

				if (colliders.empty())
					continue;

				m_PhysicsWorld->CreateBody(
					entity.GetUUID(),
					entity.GetComponent<RigidBodyComponent>(),
					entity.GetComponent<TransformComponent>(),
					colliders);
			}
		}

		if (!ScriptEngine::IsInitialized())
			return;

		auto& scriptEngine = ScriptEngine::Get();
		scriptEngine.SetActivePhysicsWorld(m_PhysicsWorld);

		const auto view = m_Registry.view<ScriptComponent>();
		for (const auto e : view)
		{
			Entity entity(e, this);
			scriptEngine.OnCreateEntity(entity);
		}
	}

	auto Scene::OnRuntimeStop() -> void
	{
		EP_PROFILE_FN("Scene::OnRuntimeStop");

		// The script engine's WeakRef expires with this reset.
		m_PhysicsWorld.reset();

		if (!ScriptEngine::IsInitialized())
			return;

		auto& scriptEngine = ScriptEngine::Get();
		const auto view = m_Registry.view<ScriptComponent>();
		for (const auto e : view)
		{
			Entity entity(e, this);
			scriptEngine.OnDestroyEntity(entity);
		}
	}

	auto Scene::OnUpdateRuntime(float timestep) -> void
	{
		EP_PROFILE_FN("Scene::OnUpdateRuntime");

		// Step physics before scripts so they observe this frame's poses (and before
		// the scripting early-return so it runs without scripts).
		if (m_PhysicsWorld)
		{
			m_PhysicsWorld->Step(timestep);

			const auto view = m_Registry.view<RigidBodyComponent, TransformComponent>();
			for (const auto e : view)
			{
				Entity entity(e, this);
				const UUID id = entity.GetUUID();
				if (!m_PhysicsWorld->HasBody(id))
					continue;

				auto& tc = entity.GetComponent<TransformComponent>();
				tc.Translation = m_PhysicsWorld->GetPosition(id);
				tc.Rotation = glm::eulerAngles(m_PhysicsWorld->GetRotation(id));
			}
		}

		if (!ScriptEngine::IsInitialized())
			return;

		auto& scriptEngine = ScriptEngine::Get();
		const auto view = m_Registry.view<ScriptComponent>();
		for (const auto e : view)
		{
			Entity entity(e, this);
			scriptEngine.OnUpdateEntity(entity, timestep);
		}
	}

	auto Scene::OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const ScopedPtr<EditorCamera>& camera, EntityHandle selectedEntity) -> void
	{
		EP_PROFILE_FN("Scene::OnRenderEditor");

		sceneRenderer->BeginScene(camera);
		RenderScene(sceneRenderer);

		// Wireframe overlay: redraw the selected entity's mesh as wireframe.
		// Only draws in editor mode; runtime rendering ignores selection.
		const Entity entity = selectedEntity != entt::null ? Entity{selectedEntity, this} : Entity{};
		if (entity && entity.HasComponent<MeshComponent>())
		{
			const auto& mc = entity.GetComponent<MeshComponent>();
			if (mc.MeshHandle)
				sceneRenderer->SubmitWireframeMesh(mc.MeshHandle, GetWorldTransform(entity));
		}

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

		// Derive the view from translation + rotation only; a scaled camera entity
		// must not distort the view, so scale is intentionally ignored here.
		const glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation)
			* glm::mat4_cast(glm::quat(transform.Rotation));

		sceneRenderer->BeginScene(glm::inverse(cameraTransform), camera.GetProjectionMatrix(), transform.Translation);
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
		entity.AddComponent<RelationshipComponent>();

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

	auto Scene::DuplicateEntity(Entity entity) -> Entity
	{
		EP_PROFILE_FN("Scene::DuplicateEntity");

		const std::string& name = entity.GetName();
		const Entity newEntity = CreateEntity(name);

		TryCopyComponent<TransformComponent>(entity, newEntity);
		TryCopyComponent<MeshComponent>(entity, newEntity);
		TryCopyComponent<CameraComponent>(entity, newEntity);
		TryCopyComponent<PointLightComponent>(entity, newEntity);
		TryCopyComponent<ScriptComponent>(entity, newEntity);
		TryCopyComponent<RigidBodyComponent>(entity, newEntity);
		TryCopyComponent<BoxColliderComponent>(entity, newEntity);
		TryCopyComponent<SphereColliderComponent>(entity, newEntity);
		TryCopyComponent<CapsuleColliderComponent>(entity, newEntity);

		// Script field values live in ScriptEngine's side table, keyed by UUID,
		// so they must be copied across to the new entity explicitly.
		if (entity.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
			ScriptEngine::Get().CopyFieldMap(entity.GetUUID(), newEntity.GetUUID());

		// Attach the copy to the source's parent as a leaf sibling. Set here rather
		// than blind-copied above, which would claim the source's children.
		if (const UUID parentId = entity.GetComponent<RelationshipComponent>().Parent)
		{
			newEntity.GetComponent<RelationshipComponent>().Parent = parentId;
			if (const Entity parent = GetEntityByUUID(parentId))
				parent.GetComponent<RelationshipComponent>().Children.push_back(newEntity.GetUUID());
		}

		return newEntity;
	}

	auto Scene::DestroyEntity(Entity entity) -> void
	{
		// Detach from the parent so its child list stays valid, then destroy the subtree.
		if (const UUID parentId = entity.GetComponent<RelationshipComponent>().Parent)
		{
			if (const Entity parent = GetEntityByUUID(parentId))
			{
				auto& siblings = parent.GetComponent<RelationshipComponent>().Children;
				std::erase(siblings, entity.GetUUID());
			}
		}

		DestroyEntityHierarchy(entity);
	}

	auto Scene::DestroyEntityHierarchy(Entity entity) -> void
	{
		// Snapshot the child list: recursing destroys entities and frees the component.
		const std::vector<UUID> children = entity.GetComponent<RelationshipComponent>().Children;
		for (const UUID childId : children)
		{
			if (const Entity child = GetEntityByUUID(childId))
				DestroyEntityHierarchy(child);
		}

		if (entity.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
			ScriptEngine::Get().RemoveFieldMap(entity.GetUUID());

		m_EntityMap.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	auto Scene::SetParent(Entity child, Entity parent) -> void
	{
		EP_PROFILE_FN("Scene::SetParent");

		auto& childRelationship = child.GetComponent<RelationshipComponent>();

		// Reject cycles: walking up from the new parent must not reach the child.
		for (Entity ancestor = parent; ancestor; )
		{
			if (ancestor == child)
			{
				Log::Warn("Ignoring reparent of '{}': target is itself or a descendant.", child.GetName());
				return;
			}

			const UUID ancestorParent = ancestor.GetComponent<RelationshipComponent>().Parent;
			ancestor = ancestorParent ? GetEntityByUUID(ancestorParent) : Entity{};
		}

		// Capture the world transform to preserve it across the parent change.
		const glm::mat4 worldTransform = GetWorldTransform(child);

		// Unlink from the current parent, if any.
		if (const UUID oldParentId = childRelationship.Parent)
		{
			if (const Entity oldParent = GetEntityByUUID(oldParentId))
				std::erase(oldParent.GetComponent<RelationshipComponent>().Children, child.GetUUID());
		}

		// Link to the new parent (or become a root when parent is invalid).
		childRelationship.Parent = parent ? parent.GetUUID() : UUID(0);
		if (parent)
			parent.GetComponent<RelationshipComponent>().Children.push_back(child.GetUUID());

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

	auto Scene::GetWorldTransform(Entity entity) -> glm::mat4
	{
		glm::mat4 world(1.0f);

		// Walk child -> root, pre-multiplying each local transform. The guard defends
		// against a cycle in malformed serialized data.
		Entity current = entity;
		for (size_t guard = 0; current; ++guard)
		{
			world = current.GetComponent<TransformComponent>().GetTransform() * world;

			const UUID parentId = current.GetComponent<RelationshipComponent>().Parent;
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

	template<typename T>
	auto Scene::TryCopyComponent(Entity srcEntity, Entity dstEntity) -> void
	{
		EP_PROFILE_FN("Scene::TryCopyComponent");

		if (srcEntity.HasComponent<T>())
			dstEntity.AddOrReplaceComponent<T>(srcEntity.GetComponent<T>());
	}

	template<typename T>
	auto Scene::CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, EntityHandle>& entityMap) -> void
	{
		EP_PROFILE_FN("Scene::CopyComponent");

		auto view = srcRegistry.view<T>();
		for (auto srcEntity : view)
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
		const std::vector<EntityHandle> srcEntities(idView.begin(), idView.end());
		for (auto it = srcEntities.rbegin(); it != srcEntities.rend(); ++it)
		{
			const auto entity = *it;
			auto uuid = srcRegistry.get<IDComponent>(entity).ID;
			const auto& name = srcRegistry.get<TagComponent>(entity).Tag;
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			entityMap[uuid] = newEntity;
		}

		CopyComponent<TransformComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<MeshComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<CameraComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<PointLightComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<ScriptComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<RigidBodyComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<BoxColliderComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<SphereColliderComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<CapsuleColliderComponent>(srcRegistry, dstRegistry, entityMap);
		// UUID-based links copy verbatim, no handle remapping needed.
		CopyComponent<RelationshipComponent>(srcRegistry, dstRegistry, entityMap);

		newScene->m_Environment = scene->m_Environment;

		return newScene;
	}

	auto Scene::RenderScene(const Ref<SceneRenderer>& sceneRenderer) -> void
	{
		EP_PROFILE_FN("Scene::RenderScene");

		sceneRenderer->SubmitEnvironment(m_Environment);

		const auto lightView = m_Registry.view<PointLightComponent, TransformComponent>();
		for (const auto& entity : lightView)
		{
			const auto& lightComponent = lightView.get<PointLightComponent>(entity);
			// World-space translation, so a parented light follows its parent.
			const glm::vec3 worldPosition = glm::vec3(GetWorldTransform(Entity(entity, this))[3]);
			sceneRenderer->SubmitPointLight(worldPosition, lightComponent.Color, lightComponent.Intensity);
		}

		const auto view = m_Registry.view<MeshComponent, TransformComponent>();
		for (const auto& entity : view)
		{
			if (const auto& meshComponent = view.get<MeshComponent>(entity); meshComponent.MeshHandle)
			{
				sceneRenderer->SubmitMesh(meshComponent.MeshHandle, GetWorldTransform(Entity(entity, this)));
			}
		}
	}
}