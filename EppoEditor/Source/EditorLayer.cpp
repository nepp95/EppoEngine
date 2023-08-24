#include "EditorLayer.h"

#include <imgui/imgui.h>
#include <backends/imgui_impl_vulkan.h>

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
		m_ActiveScene->Render(m_EditorCamera);
	}

	void EditorLayer::RenderGui()
	{
		ImGui::Begin("Viewport");

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		m_ViewportWidth = viewportSize.x;
		m_ViewportHeight = viewportSize.y;

		ImGui::Text("This is text");

		UI::Image(m_ActiveScene->GetFinalImage(), ImGui::GetContentRegionAvail());

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e)
	{

	}
}
