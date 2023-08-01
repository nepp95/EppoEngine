#include "pch.h"
#include "Scene.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	void Scene::OnUpdate(float timestep)
	{

	}

	void Scene::Render()
	{
		Renderer::BeginScene();

		Renderer::SubmitCommand([]()
		{
			Renderer::DrawQuad({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
			Renderer::DrawQuad({ 0.5f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f });
			Renderer::DrawQuad({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f });
		});

		Renderer::EndScene();
	}
}
