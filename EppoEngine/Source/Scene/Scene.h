#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Scene/Components.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Eppo
{
	using EntityHandle = entt::entity;
	class Entity;
	class SceneRenderer;
	class PhysicsWorld;

	// Scene-level lighting environment. Without a skybox image the renderer
	// shades the background and the ambient term from these three colors (a
	// vertical zenith->horizon->ground gradient). SkyboxHandle is the seam for a
	// future equirectangular HDR: when it resolves to a loaded image the renderer
	// samples that instead of the gradient. Colors are authored in linear space.
	struct EnvironmentSettings
	{
		AssetHandle SkyboxHandle = 0;
		glm::vec3 ZenithColor = { 0.35f, 0.45f, 0.55f };
		glm::vec3 HorizonColor = { 0.65f, 0.66f, 0.67f };
		glm::vec3 GroundColor = { 0.20f, 0.17f, 0.13f };
		float AmbientIntensity = 1.0f;
	};

	class Scene : public Asset
	{
	public:
		Scene() = default;
		~Scene() override = default;

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

		// Reparents child under parent (invalid parent detaches to root), preserving
		// the child's world transform. No-op if the move would create a cycle.
		auto SetParent(Entity child, Entity parent) -> void;

		// Composes an entity's world transform from its parent chain.
		[[nodiscard]] auto GetWorldTransform(Entity entity) -> glm::mat4;

		// Resolve an entity by its stable UUID. Returns an invalid Entity if the
		// UUID is not present in this scene. Used to remap a selection across the
		// editor/runtime scene copies (UUIDs survive Scene::Copy, handles do not).
		[[nodiscard]] auto GetEntityByUUID(const UUID& uuid) -> Entity;

		// Invokes `func` for every entity. Used by the editor to gather debug-draw
		// items (e.g. collider wireframes) without exposing the underlying registry.
		template<typename Func>
		auto ForEachEntity(Func&& func) -> void
		{
			// Every entity carries an IDComponent, so a view over it enumerates all.
			for (const auto view = m_Registry.view<IDComponent>(); const auto e : view)
				func(Entity{e, this});
		}

		template<typename T>
		static auto TryCopyComponent(Entity srcEntity, Entity dstEntity) -> void;

		template<typename T>
		static auto CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, EntityHandle>& entityMap) -> void;

		static auto Copy(const Ref<Scene>& scene) -> Ref<Scene>;

		[[nodiscard]] auto GetEnvironment() -> EnvironmentSettings& { return m_Environment; }
		[[nodiscard]] auto GetEnvironment() const -> const EnvironmentSettings& { return m_Environment; }

	private:
		auto RenderScene(const Ref<SceneRenderer>& sceneRenderer) -> void;

		// Destroys an entity and its descendants (caller detaches the subtree root).
		auto DestroyEntityHierarchy(Entity entity) -> void;

	private:
		entt::registry m_Registry;
		std::unordered_map<UUID, EntityHandle> m_EntityMap;
		EnvironmentSettings m_Environment;
		Ref<PhysicsWorld> m_PhysicsWorld;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
	};
}