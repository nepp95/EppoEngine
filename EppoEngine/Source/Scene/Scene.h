#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Scene/Components.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <functional>

namespace Eppo
{
	using EntityHandle = entt::entity;
	class Entity;
	class SceneRenderer;
	class PhysicsWorld;
	class SceneSerializer;

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

	class Scene : public Asset, public std::enable_shared_from_this<Scene>
	{
	public:
		Scene() = default;
		~Scene() override = default;

		static auto GetStaticType() -> AssetType { return AssetType::Scene; }

		auto SetViewportSize(uint32_t width, uint32_t height) -> void;

		auto OnRuntimeStart() -> void;
		auto OnRuntimeStop() -> void;
		auto OnUpdateRuntime(float timestep) -> void;

		auto OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& camera) -> void;
		auto OnRenderRuntime(const Ref<SceneRenderer>& sceneRenderer) -> void;

		[[nodiscard]] auto GetPrimaryCameraEntity() -> Entity;

		auto CreateEntity(const std::string& name = std::string()) -> Entity;
		auto CreateEntityWithUUID(const UUID& uuid, const std::string& name) -> Entity;
		auto DuplicateEntity(Entity entity) -> Entity;
		auto DestroyEntity(Entity entity) -> void;
		// Queues an entity for destruction, carried out after the script update loop.
		// Safe to call from a script's OnUpdate, mid-iteration of the script view.
		// Drained only by OnUpdateRuntime, so a non-runtime caller would leave the
		// queue pending until the next runtime update.
		auto DestroyEntityDeferred(Entity entity) -> void;
		auto FitColliderToMesh(Entity entity, BoxColliderComponent& collider) -> void;
		auto FitColliderToMesh(Entity entity, SphereColliderComponent& collider) -> void;
		auto FitColliderToMesh(Entity entity, CapsuleColliderComponent& collider) -> void;
		auto FitColliderToMesh(Entity entity, CylinderColliderComponent& collider) -> void;

		// Reparents child under parent (invalid parent detaches to root), preserving
		// the child's world transform. No-op if the move would create a cycle.
		auto SetParent(Entity child, Entity parent) -> void;

		// Composes an entity's world transform from its parent chain.
		[[nodiscard]] auto GetWorldTransform(Entity entity) -> glm::mat4;

		// Enumerate every entity in creation order, handing each to `func`. Every
		// entity carries an IDComponent, so a view over it covers the whole scene.
		// Use this instead of reaching into the registry for all-entity iteration.
		auto ForEachEntity(const std::function<void(Entity)>& func) -> void;

		// Sort entities by UUID for deterministic iteration (e.g. serialization).
		// Subsequent ForEachEntity calls visit them in this order.
		auto SortEntitiesByID() -> void;

		// Resolve an entity by its stable UUID. Returns an invalid Entity if the
		// UUID is not present in this scene. Used to remap a selection across the
		// editor/runtime scene copies (UUIDs survive Scene::Copy, handles do not).
		[[nodiscard]] auto GetEntityByUUID(const UUID& uuid) -> Entity;

		[[nodiscard]] auto FindEntityByName(const std::string& name) -> Entity;

		template<typename T>
		static auto TryCopyComponent(Entity srcEntity, Entity dstEntity) -> void;

		template<typename T>
		static auto CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, EntityHandle>& entityMap) -> void;

		static auto Copy(const Ref<Scene>& scene) -> Ref<Scene>;

		[[nodiscard]] auto GetEnvironment() -> EnvironmentSettings& { return m_Environment; }
		[[nodiscard]] auto GetEnvironment() const -> const EnvironmentSettings& { return m_Environment; }

		// Null outside runtime (between OnRuntimeStop and the next OnRuntimeStart).
		[[nodiscard]] auto GetPhysicsWorld() const -> Ref<PhysicsWorld> { return m_PhysicsWorld; }
		[[nodiscard]] auto GetColliderlessRigidBodies() const -> const std::vector<std::string>& { return m_ColliderlessRigidBodies; }

	private:
		auto RenderScene(const Ref<SceneRenderer>& sceneRenderer) -> void;

		// Destroys an entity and its descendants (caller detaches the subtree root).
		auto DestroyEntityHierarchy(Entity entity) -> void;

		// Destroys everything queued via DestroyEntityDeferred this frame.
		auto FlushDestroyQueue() -> void;

	private:
		entt::registry m_Registry;
		std::unordered_map<UUID, EntityHandle> m_EntityMap;
		EnvironmentSettings m_Environment;
		Ref<PhysicsWorld> m_PhysicsWorld;
		std::vector<std::string> m_ColliderlessRigidBodies;
		std::vector<UUID> m_EntitiesToDestroy;

		friend class Entity;
	};
}
