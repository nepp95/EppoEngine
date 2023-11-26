#pragma once

#include "Core/UUID.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Image.h"

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

		void OnUpdate(float timestep);
		void Render(const EditorCamera& editorCamera);
		void RenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& editorCamera);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name);
		void DestroyEntity(Entity entity);

	private:
		entt::registry m_Registry;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
	};
}
