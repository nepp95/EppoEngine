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
		m_TestTexture.reset();
	}
	
	void EditorLayer::Update(float timestep)
	{
		m_ActiveScene->OnUpdate(timestep);
	}
	
	void EditorLayer::Render()
	{
		m_ActiveScene->Render();
	}

	void EditorLayer::OnEvent(Event& e)
	{

	}
}
