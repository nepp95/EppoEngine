#include "EditorLayer.h"

#include "Panel/SceneHierarchyPanel.h"

#include <imgui/imgui.h>

namespace Eppo
{
	static const std::string SCENE_HIERARCHY_PANEL = "SceneHierarchyPanel";

	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{}

	void EditorLayer::OnAttach()
	{
		m_ActiveScene = CreateRef<Scene>();

		{
			Entity entity = m_ActiveScene->CreateEntity("Entity 1");
			entity.AddComponent<ColorComponent>(glm::vec4(0.9f, 0.2f, 0.2f, 1.0f));
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = { -0.5f, -0.5f, 0.0f };
		}

		{
			Entity entity = m_ActiveScene->CreateEntity("Entity 2");
			entity.AddComponent<ColorComponent>(glm::vec4(0.2f, 0.9f, 0.2f, 1.0f));
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = { 0.5f, 0.5f, 0.0f };
		}

		{
			Entity entity = m_ActiveScene->CreateEntity("Entity 3");
			entity.AddComponent<ColorComponent>(glm::vec4(0.2f, 0.2f, 0.9f, 1.0f));
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = { -0.5f, 0.5f, 0.0f };
		}

		{
			Entity entity = m_ActiveScene->CreateEntity("Entity 4");
			entity.AddComponent<ColorComponent>(glm::vec4(0.2f, 0.5f, 0.5f, 1.0f));
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = { 0.5f, -0.5f, 0.0f };
		}

		m_TestTexture = CreateRef<Texture>("Resources/Textures/Icons/Directory.png");

		m_PanelManager = CreateScope<PanelManager>();
		m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL, true);

		m_PanelManager->SetSceneContext(m_ActiveScene);
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
		// ImGui docking example
		static bool dockspaceOpen = true;
		static bool optFullscreenPersistence = true;
		bool optFullscreen = optFullscreenPersistence;
		static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (optFullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar(); // ImGuiStyleVar_WindowPadding

		if (optFullscreen)
			ImGui::PopStyleVar(2); // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowRounding

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		style.WindowMinSize.x = minWinSizeX;

		// Viewport
		ImGui::Begin("Viewport");

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		m_ViewportWidth = viewportSize.x;
		m_ViewportHeight = viewportSize.y;

		UI::Image(m_ActiveScene->GetFinalImage(), ImGui::GetContentRegionAvail());

		ImGui::End(); // Viewport

		// Panels
		m_PanelManager->RenderGui();

		// Settings
		ImGui::Begin("Settings");
		ImGui::Text("Here I will put settings... Sometime... Never...");
		ImGui::End(); // Settings

		ImGui::End(); // DockSpace
	}

	void EditorLayer::OnEvent(Event& e)
	{

	}
}
