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

		for (const EntityHandle entity : group)
		{
			auto [transform, color] = group.get<TransformComponent, ColorComponent>(entity);
			Renderer::DrawQuad(transform.GetTransform(), color.Color);
		}

		Renderer::EndScene();
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("Scene::CreateEntity");

		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("Scene::CreateEntityWithUUID");

		Entity entity(m_Registry.create(), this);

		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();

		auto& tag = entity.AddComponent<TagComponent>();
		tag = name.empty() ? "Entity" : name;

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		EPPO_PROFILE_FUNCTION("Scene::DestroyEntity");

		m_Registry.destroy(entity);
	}

	Ref<Image> Scene::GetFinalImage() const
	{
		EPPO_PROFILE_FUNCTION("Scene::GetFinalImage");

		return Renderer::GetFinalImage();
	}
}
