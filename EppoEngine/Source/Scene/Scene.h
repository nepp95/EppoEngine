#pragma once

#include "Core/UUID.h"
#include "Renderer/Camera/EditorCamera.h"

#include <entt/entt.hpp>

namespace Eppo
{
	class Entity;
	class SceneRenderer;

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		void OnUpdateRuntime(float timestep);
		void RenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& editorCamera);

		void OnRuntimeStart();
		void OnRuntimeStop();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name);
		void DestroyEntity(Entity entity);

	private:
		void OnPhysicsStart();
		void OnPhysicsStop();

	private:
		entt::registry m_Registry;

		bool m_IsRunning = false;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
	};
}
