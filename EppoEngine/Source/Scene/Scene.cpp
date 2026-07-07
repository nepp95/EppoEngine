#include "pch.h"
#include "Scene/Scene.h"

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

		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		m_EntityMap[uuid] = entity;

		return entity;
	}

	auto Scene::DuplicateEntity(Entity entity) -> Entity
	{
		EP_PROFILE_FN("Scene::DuplicateEntity");

		const std::string& name = entity.GetName();
		const Entity newEntity = CreateEntity(name);

		TryCopyComponent<TransformComponent>(entity, newEntity);
		TryCopyComponent<MeshComponent>(entity, newEntity);
		TryCopyComponent<CameraComponent>(entity, newEntity);
		TryCopyComponent<ScriptComponent>(entity, newEntity);

		// Script field values live in ScriptEngine's side table, keyed by UUID,
		// so they must be copied across to the new entity explicitly.
		if (entity.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
			ScriptEngine::Get().CopyFieldMap(entity.GetUUID(), newEntity.GetUUID());

		return newEntity;
	}

	auto Scene::DestroyEntity(Entity entity) -> void
	{
		if (entity.HasComponent<ScriptComponent>() && ScriptEngine::IsInitialized())
			ScriptEngine::Get().RemoveFieldMap(entity.GetUUID());

		if (m_EntityMap.contains(entity.GetUUID()))
			m_EntityMap.erase(entity.GetUUID());

		m_Registry.destroy(entity);
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

		for (const auto entity : idView)
		{
			auto uuid = srcRegistry.get<IDComponent>(entity).ID;
			const auto& name = srcRegistry.get<TagComponent>(entity).Tag;
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			entityMap[uuid] = newEntity;
		}

		CopyComponent<TransformComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<MeshComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<CameraComponent>(srcRegistry, dstRegistry, entityMap);
		CopyComponent<ScriptComponent>(srcRegistry, dstRegistry, entityMap);

		return newScene;
	}

	auto Scene::RenderScene(const Ref<SceneRenderer>& sceneRenderer) -> void
	{
		EP_PROFILE_FN("Scene::RenderScene");

		const auto view = m_Registry.view<MeshComponent, TransformComponent>();
		for (const auto& entity : view)
		{
			if (auto [transformComponent, meshComponent] = view.get<TransformComponent, MeshComponent>(entity); meshComponent.MeshHandle)
			{
				sceneRenderer->SubmitMesh(meshComponent.MeshHandle, transformComponent.GetTransform());
			}
		}
	}
}