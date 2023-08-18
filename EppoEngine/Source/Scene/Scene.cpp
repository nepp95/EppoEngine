#include "pch.h"
#include "Scene.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	void Scene::OnUpdate(float timestep)
	{
		EPPO_PROFILE_FUNCTION("Scene::OnUpdate");
	}

	void Scene::Render()
	{
		EPPO_PROFILE_FUNCTION("Scene::Render");

		/*Renderer::BeginScene();

		Renderer::DrawQuad({ -0.5f, -0.5f, 0.0f }, { 0.9f, 0.2f, 0.2f, 1.0f });
		Renderer::DrawQuad({ 0.5f, 0.5f, 0.0f }, { 0.2f, 0.9f, 0.2f, 1.0f });
		Renderer::DrawQuad({ -0.5f, 0.5f, 0.0f }, { 0.2f, 0.2f, 0.9f, 1.0f });
		Renderer::DrawQuad({ 0.5f, -0.5f, 0.0f }, { 0.2f, 0.5f, 0.5f, 1.0f });

		Renderer::EndScene();*/
	}
}
