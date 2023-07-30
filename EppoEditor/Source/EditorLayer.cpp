#include "EditorLayer.h"

namespace Eppo
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{

	}

	void EditorLayer::OnAttach()
	{
	}
	
	void EditorLayer::OnDetach()
	{
	}
	
	void EditorLayer::Update(float timestep)
	{
	}
	
	void EditorLayer::Render()
	{
		Renderer::DrawSomething();
	}

	void EditorLayer::OnEvent(Event& e)
	{

	}
}
