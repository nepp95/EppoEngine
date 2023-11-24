#include "pch.h"
#include "Scene.h"

#include "Asset/AssetManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Entity.h"

namespace Eppo
{
	void Scene::OnUpdate(float timestep)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnUpdate");
		EPPO_PROFILE_FN("CPU Update", "Update Scene");
	}

	void Scene::Render(const EditorCamera& editorCamera)
	{
		EPPO_PROFILE_FUNCTION("Scene::Render");
		EPPO_PROFILE_FN("CPU Render", "Render Scene");

		Renderer::BeginScene(editorCamera);

		{
			auto view = m_Registry.view<TransformComponent, SpriteComponent>();

			for (const EntityHandle entity : view)
			{
				auto [transform, sprite] = view.get<TransformComponent, SpriteComponent>(entity);
				Renderer::DrawQuad(transform.GetTransform(), sprite, (int)entity);
			}
		}

		{
			auto view = m_Registry.view<TransformComponent, MeshComponent>();

			for (const EntityHandle entity : view)
			{
				auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
				if (mesh.MeshHandle)
					Renderer::SubmitGeometry(transform.GetTransform(), mesh);
			}
		}

		Renderer::EndScene();
	}

	void Scene::RenderEditor(const Ref<SceneRenderer>& sceneRenderer)
	{
		sceneRenderer->BeginScene();

		{
			auto view = m_Registry.view<MeshComponent, TransformComponent>();

			for (const EntityHandle entity : view)
			{
				auto [meshC, transform] = view.get<MeshComponent, TransformComponent>(entity);
				if (meshC.MeshHandle)
				{
					Ref<Mesh> mesh = AssetManager::Get().GetAsset<Mesh>(meshC.MeshHandle);
					sceneRenderer->SubmitMesh(transform.GetTransform(), mesh, entity);
				}
			}
		}

		sceneRenderer->EndScene();
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
