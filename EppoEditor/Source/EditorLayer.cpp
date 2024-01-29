#include "EditorLayer.h"

#include "Panel/ContentBrowserPanel.h"
#include "Panel/PropertyPanel.h"
#include "Panel/SceneHierarchyPanel.h"

#include <imgui/imgui.h>

namespace Eppo
{
	static const std::string CONTENT_BROWSER_PANEL = "ContentBrowserPanel";
	static const std::string PROPERTY_PANEL = "PropertyPanel";
	static const std::string SCENE_HIERARCHY_PANEL = "SceneHierarchyPanel";

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_PanelManager(PanelManager::Get())
	{}

	void EditorLayer::OnAttach()
	{
		// Setup UI panels
		m_PanelManager.AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL, true, m_PanelManager);
		m_PanelManager.AddPanel<PropertyPanel>(PROPERTY_PANEL, true, m_PanelManager);
		m_PanelManager.AddPanel<ContentBrowserPanel>(CONTENT_BROWSER_PANEL, true, m_PanelManager);

		m_PanelManager.SetSceneContext(m_ActiveScene);

		// Open scene
		//OpenScene("Resources/Scenes/Test.epposcene");
		NewScene();

		m_ViewportRenderer = CreateRef<SceneRenderer>(m_ActiveScene, RenderSpecification());
	}
	
	void EditorLayer::OnDetach()
	{
		AssetManager::Get().Shutdown();
	}
	
	void EditorLayer::Update(float timestep)
	{
		m_EditorCamera.OnUpdate(timestep);
		m_ActiveScene->OnUpdate(timestep);
	}
	
	void EditorLayer::Render()
	{
		m_ActiveScene->RenderEditor(m_ViewportRenderer, m_EditorCamera);
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

		// Menubar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save Scene", "CTRL+S"))
					SaveScene();

				if (ImGui::MenuItem("Open Scene", "CTRL+O"))
					OpenScene();

				if (ImGui::MenuItem("Exit"))
					Application::Get().Close();

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// Viewport
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0 ));
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		m_ViewportWidth = viewportSize.x;
		m_ViewportHeight = viewportSize.y;

		UI::Image(m_ViewportRenderer->GetFinalPassImage(), ImGui::GetContentRegionAvail());

		ImGui::End(); // Viewport
		ImGui::PopStyleVar();

		// Panels
		m_PanelManager.RenderGui();

		// Performance
		m_ViewportRenderer->RenderGui();
		/*ImGui::Begin("Performance");

		const auto& profileData = Application::Get().GetProfiler()->GetProfileData();
		for (const auto& [category, results] : profileData)
		{
			ImGui::Text("%s", category.c_str());

			std::chrono::microseconds totalCategoryTime = std::chrono::microseconds::zero();

			for (const auto& [tag, time] : results)
			{
				ImGui::Text("  %s: %.3fms", tag.c_str(), time.count() / 1000.0f);
				totalCategoryTime += time;
			}

			ImGui::Text("Total time: %.3fms", totalCategoryTime.count() / 1000.0f);
			ImGui::Separator();
		}

		Application::Get().GetProfiler()->Clear();

		ImGui::End();*/ // Performance

		ImGui::End(); // DockSpace
	}

	void EditorLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(EditorLayer::OnKeyPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		if (e.IsRepeat())
			return false;

		bool alt = Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt);
		bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		switch (e.GetKeyCode())
		{
			case Key::O:
			{
				if (control)
					OpenScene();
				break;
			}

			case Key::S:
			{
				if (control)
					SaveScene();
				break;
			}
		}

		return false;
	}

	void EditorLayer::NewScene()
	{
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScenePath = std::filesystem::path();

		m_PanelManager.SetSceneContext(m_ActiveScene);
	}

	void EditorLayer::SaveScene()
	{
		if (m_ActiveScenePath.empty())
			m_ActiveScenePath = FileDialog::SaveFile("EppoEngine Scene (*.epposcene)\0*.epposcene\0");

		SaveScene(m_ActiveScenePath);
	}

	void EditorLayer::SaveScene(const std::filesystem::path& filepath)
	{
		SceneSerializer serializer(m_ActiveScene);
		serializer.Serialize(m_ActiveScenePath);
	}

	void EditorLayer::OpenScene()
	{
		std::filesystem::path filepath = FileDialog::OpenFile("EppoEngine Scene (*.epposcene)\0*.epposcene\0");

		if (!filepath.empty())
			OpenScene(filepath);
	}

	void EditorLayer::OpenScene(const std::filesystem::path& filepath)
	{
		if (filepath.extension().string() != ".epposcene")
		{
			EPPO_ERROR("Could not load '{}' because it is not a scene file!", filepath.string());
			return;
		}

		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);

		if (serializer.Deserialize(filepath))
		{
			m_ActiveScene = newScene;
			m_ActiveScenePath = filepath;
			
			m_PanelManager.SetSceneContext(m_ActiveScene);
		}
	}
}
