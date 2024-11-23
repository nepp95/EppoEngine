#include "EditorLayer.h"

#include "Asset/AssetManagerEditor.h"
#include "Panel/ContentBrowserPanel.h"
#include "Panel/PropertyPanel.h"
#include "Panel/SceneHierarchyPanel.h"

#include <imgui/imgui.h>

#include <fstream>

namespace Eppo
{
	static const std::string CONTENT_BROWSER_PANEL = "ContentBrowserPanel";
	static const std::string PROPERTY_PANEL = "PropertyPanel";
	static const std::string SCENE_HIERARCHY_PANEL = "SceneHierarchyPanel";

	static bool s_NewProjectPopup = false;
	static bool s_PreferencesPopup = false;

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_PanelManager(PanelManager::Get())
	{}

	void EditorLayer::OnAttach()
	{
		// Load resources
		m_IconPlay = Image::Create(ImageSpecification("Resources/Textures/Icons/PlayButton.png"));
		m_IconStop = Image::Create(ImageSpecification("Resources/Textures/Icons/StopButton.png"));

		// Setup UI panels
		m_PanelManager.AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL, true, m_PanelManager);
		m_PanelManager.AddPanel<PropertyPanel>(PROPERTY_PANEL, true, m_PanelManager);

		m_PanelManager.SetSceneContext(m_EditorScene);

		// Open scene
		OpenProject();

		RenderSpecification renderSpec;
		renderSpec.Width = 1600;
		renderSpec.Height = 900;
		renderSpec.DebugRendering = true;

		m_ViewportRenderer = SceneRenderer::Create(m_EditorScene, renderSpec);
	}
	
	void EditorLayer::OnDetach()
	{
		CloseProject();

		m_PanelManager.Shutdown();

		m_IconPlay = nullptr;
		m_IconStop = nullptr;

		m_ViewportRenderer = nullptr;
	}
	
	void EditorLayer::Update(float timestep)
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{
			m_EditorCamera.SetViewportSize(glm::vec2(m_ViewportWidth, m_ViewportHeight));
			m_ViewportRenderer->Resize(m_ViewportWidth, m_ViewportHeight);
			m_EditorScene->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_ActiveScene->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				if (m_ViewportFocused)
					m_EditorCamera.OnUpdate(timestep);
				break;
			}

			case SceneState::Play:
			{
				m_ActiveScene->OnUpdateRuntime(timestep);
				break;
			}
		}
	}
	
	void EditorLayer::Render()
	{
		if (!Project::GetActive())
			return;

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_ActiveScene->OnRenderEditor(m_ViewportRenderer, m_EditorCamera);
				break;
			}

			case SceneState::Play:
			{
				m_ActiveScene->OnRenderRuntime(m_ViewportRenderer);
				break;
			}
		}
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
				if (ImGui::MenuItem("New Project"))
					s_NewProjectPopup = true;

				if (ImGui::MenuItem("Save Project", "CTRL+S"))
					SaveProject();

				if (ImGui::MenuItem("Open Project", "CTRL+O"))
					OpenProject();

				if (ImGui::MenuItem("New Scene"))
					NewScene();

				ImGui::Separator();

				if (ImGui::MenuItem("Import asset"))
					ImportAsset();

				if (ImGui::MenuItem("Project settings"))
					s_PreferencesPopup = true;

				if (ImGui::MenuItem("Exit"))
					Application::Get().Close();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Testing"))
			{
				if (ImGui::MenuItem("Serialize asset registry"))
					Project::GetActive()->GetAssetManagerEditor()->SerializeAssetRegistry();

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// Popups
		if (s_NewProjectPopup)
		{
			ImGuiPopupFlags flags = ImGuiPopupFlags_NoOpenOverExistingPopup;
			ImGui::OpenPopup("New Project", flags);
			s_NewProjectPopup = false;
		}

		if (s_PreferencesPopup)
		{
			ImGuiPopupFlags flags = ImGuiPopupFlags_NoOpenOverExistingPopup;
			ImGui::OpenPopup("Project settings", flags);
			s_PreferencesPopup = false;
		}

		// Viewport
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0 ));
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered);

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		m_ViewportWidth = viewportSize.x;
		m_ViewportHeight = viewportSize.y;

		UI::Image(m_ViewportRenderer->GetFinalImage(), ImVec2(m_ViewportWidth, m_ViewportHeight), ImVec2(0, 1), ImVec2(1, 0));

		ImGui::End(); // Viewport
		ImGui::PopStyleVar();

		// Panels
		m_PanelManager.RenderGui();

		// Performance
		m_ViewportRenderer->RenderGui();

		UI_File_NewProject();
		UI_File_Preferences();
		UI_Toolbar();
	
		ImGui::End(); // DockSpace
	}

	void EditorLayer::OnEvent(Event& e)
	{
		if (m_SceneState == SceneState::Edit)
			m_EditorCamera.OnEvent(e);

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
					OpenProject();
				break;
			}

			case Key::S:
			{
				if (control)
					SaveProject();
				break;
			}
		}

		return false;
	}

	void EditorLayer::OnScenePlay()
	{
		if (!m_EditorScene)
			return;

		m_SceneState = SceneState::Play;
		m_ActiveScene = Scene::Copy(m_EditorScene);

		ScriptEngine::SetSceneContext(m_ActiveScene);
		m_PanelManager.SetSceneContext(m_ActiveScene);
		
		m_ActiveScene->OnRuntimeStart();
	}

	void EditorLayer::OnSceneStop()
	{
		if (!m_ActiveScene)
			return;

		m_SceneState = SceneState::Edit;

		m_ActiveScene->OnRuntimeStop();
		m_ActiveScene = m_EditorScene;

		ScriptEngine::SetSceneContext(m_EditorScene);
		m_PanelManager.SetSceneContext(m_ActiveScene);
	}

	void EditorLayer::CloseProject()
	{
		SaveProject();

		m_PanelManager.SetSceneContext(nullptr);
		ScriptEngine::SetSceneContext(nullptr);

		if (Project::GetActive())
			Project::SetActive(nullptr);

		m_EditorScene = nullptr;
		m_ActiveScene = nullptr;
	}

	static void ReplaceToken(std::string& input, const char* token, const std::string& value)
	{
		size_t pos = 0;
		while ((pos = input.find(token, pos)) != std::string::npos)
		{
			input.replace(pos, strlen(token), value);
			pos += strlen(token);
		}
	}

	void EditorLayer::NewProject(const std::string& name)
	{
		// Create project directory
		std::filesystem::path projectPath = Filesystem::GetAppRootDirectory() / "Projects" / name;
		Filesystem::CreateDirectory(projectPath);

		// Copy new project template
		Filesystem::Copy("Resources/Templates/NewProject", projectPath);

		// Create directories
		Filesystem::CreateDirectory(projectPath / "Assets" / "Scripts" / "Source");
		Filesystem::CreateDirectory(projectPath / "Assets" / "Meshes");
		Filesystem::CreateDirectory(projectPath / "Assets" / "Scenes");
		Filesystem::CreateDirectory(projectPath / "Assets" / "Textures");

		{
			std::ifstream in(projectPath / "project.epproj");
			std::stringstream ss;
			ss << in.rdbuf();
			in.close();

			std::string inputStr = ss.str();
			ReplaceToken(inputStr, "$PROJECT_NAME$", name);

			std::ofstream out(projectPath / "project.epproj");
			out << inputStr;
			out.close();

			Filesystem::Rename(projectPath, "project.epproj", name + ".epproj");
		}

		{
			Filesystem::Move(projectPath / "premake5.lua", projectPath / "Assets" / "Scripts" / "premake5.lua");

			std::ifstream in(projectPath / "Assets" / "Scripts" / "premake5.lua");
			std::stringstream ss;
			ss << in.rdbuf();
			in.close();

			std::string inputStr = ss.str();
			ReplaceToken(inputStr, "$PROJECT_NAME$", name);

			std::ofstream out(projectPath / "Assets" / "Scripts" / "premake5.lua");
			out << inputStr;
			out.close();
		}

		Filesystem::Move(projectPath / "Win-GenerateProjects.bat", projectPath / "Assets" / "Scripts" / "Win-GenerateProjects.bat");

		// Create hello world script
		Filesystem::Copy("Resources/Templates/Scripts/Main.cs", projectPath / "Assets" / "Scripts" / "Source");

		{
			std::ifstream in(projectPath / "Assets" / "Scripts" / "Source" / "Main.cs");
			std::stringstream ss;
			ss << in.rdbuf();
			in.close();

			std::string inputStr = ss.str();
			ReplaceToken(inputStr, "$PROJECT_NAME$", name);

			std::ofstream out(projectPath / "Assets" / "Scripts" / "Source" / "Main.cs");
			out << inputStr;
			out.close();
		}

		// Run premake
		std::filesystem::path batchFile = projectPath / "Assets" / "Scripts" / "Win-GenerateProjects.bat";
		
		// todo: not working
		// system(batchFile.string().c_str());

		// Open project
		OpenProject(projectPath / std::filesystem::path(name + ".epproj"));
	}

	bool EditorLayer::OpenProject()
	{
		std::filesystem::path filePath = FileDialog::OpenFile("EppoEngine Project (*.epproj)\0*.epproj\0", Project::GetProjectsDirectory());

		if (filePath.empty())
		{
			if (Project::GetActive())
				return false;
			else
			{
				s_NewProjectPopup = true;
				return true;
			}
		}

		OpenProject(filePath);

		return true;
	}

	void EditorLayer::OpenProject(const std::filesystem::path& filepath)
	{
		if (filepath.extension().string() != ".epproj")
		{
			EPPO_ERROR("Could not load '{}' because it is not a project file!", filepath.string());
			return;
		}

		if (Project::GetActive())
			CloseProject();

		if (Project::Open(filepath))
		{
			const auto& projSpec = Project::GetActive()->GetSpecification();

			std::filesystem::path scriptPath = Project::GetAssetsDirectory() / "Scripts" / "Binaries" / std::filesystem::path(projSpec.Name + ".dll");
			ScriptEngine::LoadAppAssembly(scriptPath);

			if (projSpec.StartScene.empty())
				NewScene();
			else
				OpenScene(Project::GetAssetFilepath(projSpec.StartScene));

			m_PanelManager.AddPanel<ContentBrowserPanel>(CONTENT_BROWSER_PANEL, true, m_PanelManager);
			Application::Get().GetWindow().SetWindowTitle("EppoEngine Editor - " + projSpec.Name);
		}
	}

	void EditorLayer::SaveProject()
	{
		EPPO_ASSERT(Project::GetActive());
		Project::SaveActive();
	}

	void EditorLayer::NewScene()
	{
		// TODO: Check for changes. Maybe using a list of changes considering a undo feature
		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
		m_ActiveScenePath = std::filesystem::path();

		m_PanelManager.SetSceneContext(m_ActiveScene);
	}

	void EditorLayer::OpenScene(const std::filesystem::path& filepath)
	{
		if (filepath.extension().string() != ".epscene")
		{
			EPPO_ERROR("Could not load '{}' because it is not a scene file!", filepath.string());
			return;
		}

		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);

		if (serializer.Deserialize(filepath))
		{
			m_EditorScene = newScene;
			m_ActiveScene = m_EditorScene;
			m_ActiveScenePath = filepath;
			
			m_PanelManager.SetSceneContext(m_EditorScene);
		}
	}

	void EditorLayer::OpenScene(AssetHandle handle)
	{
		EPPO_ASSERT(handle);

		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		Ref<Scene> sceneAsset = AssetManager::GetAsset<Scene>(handle);
		Ref<Scene> newScene = Scene::Copy(sceneAsset);

		m_EditorScene = newScene;
		m_ActiveScene = m_EditorScene;
		m_ActiveScenePath = Project::GetActive()->GetAssetManagerEditor()->GetFilepath(handle);
		
		m_PanelManager.SetSceneContext(m_ActiveScene);
	}

	void EditorLayer::SaveScene()
	{
		if (m_ActiveScenePath.empty())
			SaveSceneAs();
		else
			AssetImporter::ExportScene(m_ActiveScene, m_ActiveScenePath);
	}

	void EditorLayer::SaveSceneAs()
	{
		std::filesystem::path filepath = FileDialog::SaveFile("EppoEngine Scene (*.epscene)\0*.epscene\0");
		if (!filepath.empty())
		{
			m_ActiveScenePath = filepath;
			AssetImporter::ExportScene(m_ActiveScene, m_ActiveScenePath);
		}
	}

	void EditorLayer::ImportAsset()
	{
		std::filesystem::path filepath = FileDialog::OpenFile("Asset file (.epscene, .glb, .gltf, .jpeg, .jpg, .png)\0*.epscene;*.glb;*.gltf;*.jpeg;*.jpg;*.png\0\0", Project::GetAssetsDirectory());

		if (!filepath.empty())
		{
			Project::GetActive()->GetAssetManagerEditor()->ImportAsset(filepath);
		}
	}

	void EditorLayer::UI_File_NewProject()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::BeginPopupModal("New Project", (bool*)0, flags))
		{
			static char nameBuffer[200]{ 0 };
			static bool projectExists = false;

			ImGui::Text("Project Name");
			ImGui::InputText("##ProjectName", nameBuffer, 200);

			std::string projectPath = std::string(nameBuffer);
			std::filesystem::path fullProjectPath = Filesystem::GetAppRootDirectory() / "Projects" / projectPath;

			projectExists = Filesystem::Exists(fullProjectPath);

			if (Filesystem::Exists(fullProjectPath) && !projectPath.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
				ImGui::Text("Project name already exists");
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::Text("Project path: \n%s", fullProjectPath.string().c_str());
			}

			ImGui::Dummy(ImVec2(50, 20));

			if (ImGui::Button("Cancel", ImVec2(100, 30)))
				ImGui::CloseCurrentPopup();

			ImGui::SameLine();

			if (projectExists)
				ImGui::BeginDisabled();

			if (ImGui::Button("Create", ImVec2(100, 30)))
			{
				NewProject(projectPath);
				ImGui::CloseCurrentPopup();
			}

			if (projectExists)
				ImGui::EndDisabled();

			ImGui::EndPopup();
		}
	}

	void EditorLayer::UI_File_Preferences()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::BeginPopupModal("Project settings", (bool*)0, flags))
		{
			auto& spec = Project::GetActive()->GetSpecification();
			
			static std::string nameBuffer = std::string(200, ' ').replace(0, 200, spec.Name);

			ImGui::Text("Project Name");
			ImGui::InputText("##ProjectName", &nameBuffer[0], 200);

			ImGui::Text("Project Directory");
			ImGui::InputText("##ProjectDirectory", &spec.ProjectDirectory.string()[0], spec.ProjectDirectory.string().length(), ImGuiInputTextFlags_ReadOnly);

			ImGui::Text("Start Scene");
			if (ImGui::BeginCombo("##StartScene", spec.StartScene.filename().string().c_str()))
			{
				Ref<AssetManagerEditor> assetManager = Project::GetActive()->GetAssetManagerEditor();
				const auto& assetRegistry = assetManager->GetAssetRegistry();

				std::string startScene = spec.StartScene.filename().string();

				for (const auto& [handle, metadata] : assetRegistry)
				{
					if (metadata.Type != AssetType::Scene)
						continue;

					bool isSelected = startScene == metadata.GetName();

					if (ImGui::Selectable(metadata.GetName().c_str(), isSelected))
						startScene == metadata.GetName();

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::Button("Cancel", ImVec2(100, 30)))
				ImGui::CloseCurrentPopup();

			ImGui::SameLine();

			if (ImGui::Button("Apply", ImVec2(100, 30)))
			{
				// TODO: Save settings
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void EditorLayer::UI_Toolbar()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::Begin("Scene Control", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		float buttonSize = ImGui::GetWindowHeight() - 4.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (buttonSize * 0.5f));

		if (m_SceneState == SceneState::Edit)
		{
			if (UI::ImageButton("##Play", m_IconPlay, ImVec2(buttonSize, buttonSize)))
				OnScenePlay();
		}
		else if (m_SceneState == SceneState::Play)
		{
			if (UI::ImageButton("##Stop", m_IconStop, ImVec2(buttonSize, buttonSize)))
				OnSceneStop();
		}

		ImGui::PopStyleVar(3);
		ImGui::End();
	}
}
