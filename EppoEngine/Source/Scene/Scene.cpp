#include "pch.h"
#include "Scene.h"

#include "Renderer/Renderer.h"
#include "Scene/Entity.h"

namespace Eppo
{
	void Scene::OnUpdate(float timestep)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnUpdate");
	}

	void Scene::Render(const EditorCamera& editorCamera)
	{
		EPPO_PROFILE_FUNCTION("Scene::Render");

		Renderer::BeginScene(editorCamera);

		auto group = m_Registry.group<TransformComponent, ColorComponent>();

		for (const auto entity : group)
		{
			auto [transform, color] = group.get<TransformComponent, ColorComponent>(entity);
			Renderer::DrawQuad(transform.GetTransform(), color.Color);
		}

		Renderer::EndScene();
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity(m_Registry.create(), this);

		entity.AddComponent<IDComponent>();
		entity.AddComponent<TransformComponent>();

		auto& tag = entity.AddComponent<TagComponent>();
		tag = name.empty() ? "Entity" : name;

		return entity;
	}

	Ref<Image> Scene::GetFinalImage() const
	{
		return Renderer::GetFinalImage();
	}
}
