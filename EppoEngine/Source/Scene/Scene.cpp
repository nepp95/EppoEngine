#include "pch.h"
#include "Scene/Scene.h"

#include "Math/Math.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scripting/ScriptEngine.h"

namespace Eppo
{
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

		if (!ScriptEngine::IsInitialized())
			return;

		auto& scriptEngine = ScriptEngine::Get();
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

	auto Scene::OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const ScopedPtr<EditorCamera>& camera) -> void
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
		const auto view = m_Registry.view<CameraComponent>();
		for (const auto e : view)
		{
			if (view.get<CameraComponent>(e).Primary)
				return Entity(e, this);
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
		const auto it = m_EntityMap.find(uuid);
		if (it != m_EntityMap.end())
			return Entity(it->second, this);

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

		// Script field values live in ScriptEngine's side table, keyed by UUID,
		// so they must be copied across to the new entity explicitly.
		if (entity.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
			ScriptEngine::Get().CopyFieldMap(entity.GetUUID(), newEntity.GetUUID());

		// The copy joins the source's parent as a sibling with the same local
		// transform (so it lands exactly on top of the source). Children are not
		// duplicated: the copy starts as a leaf. RelationshipComponent is therefore
		// not blind-copied above, which would wrongly claim the source's children.
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
		// Detach from the parent first so the parent's child list stays valid; then
		// tear down this entity and its whole subtree.
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
		// Snapshot the child list before recursing: each child's teardown frees its
		// registry storage, so we must not hold a reference into the component while
		// entities are being destroyed.
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

		// Reject moves that would form a cycle: a node cannot become a descendant of
		// itself. Walk up from the prospective parent; if we meet the child, bail.
		for (Entity ancestor = parent; ancestor; )
		{
			if (ancestor == child)
			{
				Log::Warn("Ignoring reparent of entity '{}': target is the entity itself or one of its descendants.", child.GetName());
				return;
			}

			const UUID ancestorParent = ancestor.GetComponent<RelationshipComponent>().Parent;
			ancestor = ancestorParent ? GetEntityByUUID(ancestorParent) : Entity{};
		}

		// Capture the world transform before the parent change so we can preserve it.
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

		// Re-solve the local transform so the child stays put in world space:
		// localNew = inverse(parentWorld) * worldChild.
		const glm::mat4 parentWorld = parent ? GetWorldTransform(parent) : glm::mat4(1.0f);
		const glm::mat4 localTransform = glm::inverse(parentWorld) * worldTransform;

		auto& transform = child.GetComponent<TransformComponent>();
		Math::DecomposeTransform(localTransform, transform.Translation, transform.Rotation, transform.Scale);
	}

	auto Scene::GetWorldTransform(Entity entity) -> glm::mat4
	{
		glm::mat4 world(1.0f);

		// Walk child -> parent -> ... -> root, pre-multiplying each local transform.
		// The step guard defends against a cycle in malformed serialized data (the
		// live SetParent path already prevents cycles).
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

	auto Scene::Copy(Ref<Scene> scene) -> Ref<Scene>
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
		// Relationship links are UUID-based, so they copy verbatim: the play-mode
		// scene reproduces the same hierarchy without any handle remapping.
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
			// Light position is the translation of the composed world transform, so
			// a parented light follows its parent.
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