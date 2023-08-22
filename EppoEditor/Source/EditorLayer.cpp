#include "EditorLayer.h"

#include <imgui/imgui.h>

namespace Eppo
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{}

	void EditorLayer::OnAttach()
	{
		m_ActiveScene = CreateRef<Scene>();

		m_TestTexture = CreateRef<Texture>("Resources/Textures/Icons/Directory.png");
	}
	
	void EditorLayer::OnDetach()
	{
		m_TestTexture.reset();
	}
	
	void EditorLayer::Update(float timestep)
	{
		m_EditorCamera.OnUpdate(timestep);

		m_ActiveScene->OnUpdate(timestep);
	}
	
	void EditorLayer::Render()
	{
		m_ActiveScene->Render();

		Renderer::BeginScene(m_EditorCamera);

		Renderer::DrawQuad({ -0.5f, -0.5f, 0.0f }, { 0.9f, 0.2f, 0.2f, 1.0f });
		Renderer::DrawQuad({ 0.5f, 0.5f, 0.0f }, { 0.2f, 0.9f, 0.2f, 1.0f });
		Renderer::DrawQuad({ -0.5f, 0.5f, 0.0f }, { 0.2f, 0.2f, 0.9f, 1.0f });
		Renderer::DrawQuad({ 0.5f, -0.5f, 0.0f }, { 0.2f, 0.5f, 0.5f, 1.0f });
		Renderer::DrawQuad({ 0.0f, 0.0f, 0.0f }, m_TestTexture);

		Renderer::EndScene();
	}

	void EditorLayer::RenderGui()
	{
		ImGui::Begin("Hello");

		ImGui::Text("This is text");

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e)
	{

	}
}
