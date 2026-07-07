#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Renderer/Camera/EditorCamera.h"

#include <entt/entt.hpp>

namespace Eppo
{
	using EntityHandle = entt::entity;
	class Entity;
	class SceneRenderer;

	class Scene : public Asset
	{
	public:
		Scene() = default;
		~Scene() = default;

		static auto GetStaticType() -> AssetType { return AssetType::Scene; }

		auto SetViewportSize(uint32_t width, uint32_t height) -> void;

		auto OnRuntimeStart() -> void;
		auto OnRuntimeStop() -> void;
		auto OnUpdateRuntime(float timestep) -> void;

		auto OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const ScopedPtr<EditorCamera>& camera) -> void;
		auto OnRenderRuntime(const Ref<SceneRenderer>& sceneRenderer) -> void;

		[[nodiscard]] auto GetPrimaryCameraEntity() -> Entity;

		auto CreateEntity(const std::string& name = std::string()) -> Entity;
		auto CreateEntityWithUUID(const UUID& uuid, const std::string& name) -> Entity;
		auto DuplicateEntity(Entity entity) -> Entity;
		auto DestroyEntity(Entity entity) -> void;

		template<typename T>
		static auto TryCopyComponent(Entity srcEntity, Entity dstEntity) -> void;

		template<typename T>
		static auto CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, EntityHandle>& entityMap) -> void;

		static auto Copy(Ref<Scene> scene) -> Ref<Scene>;

	private:
		auto RenderScene(const Ref<SceneRenderer>& sceneRenderer) -> void;

	private:
		entt::registry m_Registry;
		std::unordered_map<UUID, EntityHandle> m_EntityMap;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
	};
}