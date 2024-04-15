#pragma once

#include "Core/UUID.h"
#include "Renderer/Camera/EditorCamera.h"

#include <entt/entt.hpp>

class btDiscreteDynamicsWorld;

namespace Eppo
{
	class Entity;
	class SceneRenderer;

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		void SetViewportSize(uint32_t width, uint32_t height);

		void OnUpdateRuntime(float timestep);

		void OnRenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& editorCamera);
		void OnRenderRuntime(const Ref<SceneRenderer>& sceneRenderer);

		void OnRuntimeStart();
		void OnRuntimeStop();

		static Ref<Scene> Copy(Ref<Scene> scene);

		template<typename T>
		static void TryCopyComponent(Entity srcEntity, Entity dstEntity);

		template<typename T>
		static void CopyComponent(entt::registry& srcRegistry, entt::registry& dstRegistry, const std::unordered_map<UUID, entt::entity>& entityMap);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name);
		Entity DuplicateEntity(Entity entity);
		void DestroyEntity(Entity entity);

		Entity FindEntityByUUID(UUID uuid);
		Entity FindEntityByName(const std::string& name);

		bool IsRunning() const { return m_IsRunning; }

	private:
		void OnPhysicsStart();
		void OnPhysicsStop();

		void RenderScene(Ref<SceneRenderer> sceneRenderer);

	private:
		entt::registry m_Registry;
		std::unordered_map<UUID, entt::entity> m_EntityMap;

		btDiscreteDynamicsWorld* m_PhysicsWorld = nullptr;

		bool m_IsRunning = false;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
	};
}
