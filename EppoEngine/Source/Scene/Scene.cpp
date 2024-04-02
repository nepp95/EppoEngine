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

	void Scene::RenderEditor(const Ref<SceneRenderer>& sceneRenderer, const EditorCamera& editorCamera)
	{
		sceneRenderer->BeginScene(editorCamera);

		{
			auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>();

			for (const EntityHandle entity : view)
			{
				auto [dlc, tc] = view.get<DirectionalLightComponent, TransformComponent>(entity);
				sceneRenderer->SubmitDirectionalLight(dlc);
				break;
			}
		}

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
}
