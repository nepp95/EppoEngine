#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Image.h"

#include <entt/entt.hpp>

namespace Eppo
{
	class Entity;

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		void OnUpdate(float timestep);
		void Render(const EditorCamera& editorCamera);

		Entity CreateEntity(const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		Ref<Image> GetFinalImage() const;

	private:
		entt::registry m_Registry;

		friend class Entity;
	};
}
