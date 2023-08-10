#include "EditorLayer.h"

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
		m_TestTexture = nullptr;
	}
	
	void EditorLayer::Update(float timestep)
	{
		m_ActiveScene->OnUpdate(timestep);
	}
	
	void EditorLayer::Render()
	{
		m_ActiveScene->Render();

		Renderer::BeginScene();

		Renderer::DrawQuad({ 0.0f, 0.0f, 0.0f }, m_TestTexture );

		Renderer::EndScene();
	}

	void EditorLayer::OnEvent(Event& e)
	{

	}
}
