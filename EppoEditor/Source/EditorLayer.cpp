#include "EditorLayer.h"

#include "Renderer/Image.h"

#include "Panels/ContentBrowserPanel.h"
#include "Panels/PropertyPanel.h"
#include "Panels/SceneHierarchyPanel.h"

#include <imgui_stdlib.h>

namespace Eppo
{
	namespace
	{
		constexpr const char* CONTENT_BROWSER_PANEL = "Content Browser";
		constexpr const char* PROPERTY_PANEL = "Property";
		constexpr const char* SCENE_HIERARCHY_PANEL = "Scene Hierarchy";
	}

	auto EditorLayer::OnAttach() -> void
	{
		m_PanelManager = CreateRef<PanelManager>();
		m_PanelManager->AddPanel<PropertyPanel>(PROPERTY_PANEL, true);
		m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL, true);
		m_PanelManager->AddPanel<ContentBrowserPanel>(CONTENT_BROWSER_PANEL, true);

		// Route scene opening through EditorLayer so scripting is rebuilt and the
		// editor/active scene bookkeeping stays authoritative.
		m_PanelManager->GetPanel<ContentBrowserPanel>(CONTENT_BROWSER_PANEL)
			->SetOpenSceneCallback([this](AssetHandle handle) { OpenScene(handle); });

		m_EditorCamera = CreateScopedPtr<EditorCamera>(glm::vec3(-10.0f, 1.0f, 0.0f), 0.0f, 0.0f);

		const auto loadIcon = [](const char* fileName) -> Ref<Image>
		{
			const auto path = FS::GetResourcesDirectory() / "Icons" / fileName;
			if (!FS::Exists(path))
			{
				Log::Error("Toolbar icon not found: '{}'", path);
				return nullptr;
			}

			ImageSpecification spec;
			spec.ImageFormat = nvrhi::Format::SRGBA8_UNORM;
			spec.DebugName = fileName;

			return CreateRef<Image>(spec, ImageSource(path));
		};

		m_PlayIcon = loadIcon("PlayButton.png");
		m_StopIcon = loadIcon("StopButton.png");

		if (!OpenProject())
		{
			m_ActiveScene = CreateRef<Scene>();
			m_PanelManager->SetSceneContext(m_ActiveScene);
		}

		m_SceneRenderer = CreateRef<SceneRenderer>(m_ActiveScene, m_ViewportWidth, m_ViewportHeight);
	}

	auto EditorLayer::OnDetach() -> void
	{
		ScriptEngine::Shutdown();
		Project::SetActive(nullptr);
	}

	auto EditorLayer::OnUpdate(float timestep) -> void
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{
			m_EditorCamera->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_ActiveScene->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_EditorScene->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_SceneRenderer->Resize(m_ViewportWidth, m_ViewportHeight);
		}

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_EditorCamera->OnUpdate(timestep);
				m_ActiveScene->OnRenderEditor(m_SceneRenderer, m_EditorCamera);
				break;
			}

			case SceneState::Play:
			{
				m_ActiveScene->OnUpdateRuntime(timestep);
				m_ActiveScene->OnRenderRuntime(m_SceneRenderer);
				break;
			}
		}
	}

	auto EditorLayer::OnUIRender() -> void
	{
		// From ImGui docking example
		bool dockspaceOpen = true;
		constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
			windowFlags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
		ImGui::PopStyleVar(3);

		const ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();

		const float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 200.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			const ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		style.WindowMinSize.x = minWinSizeX;

		// Menu bar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Project", "CTRL+N"))
					m_NewProjectPopup = true;

				if (ImGui::MenuItem("Save Project", "CTRL+S"))
					SaveProject();

				if (ImGui::MenuItem("Open Project", "CTRL+O"))
					OpenProject();

				if (ImGui::MenuItem("Close Project"))
					CloseProject();

				if (ImGui::MenuItem("New Scene"))
					NewScene();

				if (ImGui::MenuItem("Save Scene"))
					SaveScene();

				if (ImGui::MenuItem("Open Scene"))
					OpenScene();

				if (ImGui::MenuItem("Close"))
					Application::Get().Close();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Window"))
			{
				if (ImGui::MenuItem("Content Browser"))
					m_PanelManager->TogglePanel(CONTENT_BROWSER_PANEL);

				if (ImGui::MenuItem("Properties"))
					m_PanelManager->TogglePanel(PROPERTY_PANEL);

				if (ImGui::MenuItem("Scene Hierarchy"))
					m_PanelManager->TogglePanel(SCENE_HIERARCHY_PANEL);

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// Popups
		if (m_NewProjectPopup)
		{
			constexpr ImGuiPopupFlags popupFlags = ImGuiPopupFlags_NoOpenOverExistingPopup;
			ImGui::OpenPopup("New Project", popupFlags);
			m_NewProjectPopup = false;
		}

		UI_NewProjectPopup();

		// Scene render
		m_SceneRenderer->RenderGui();

		// Panels
		m_PanelManager->RenderGui();

		// Viewport
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		const auto& app = Application::Get();
		app.GetImGuiLayer()->BlockEvents(!m_ViewportHovered);

		const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		m_ViewportWidth = static_cast<uint32_t>(viewportSize.x);
		m_ViewportHeight = static_cast<uint32_t>(viewportSize.y);

		const auto& finalImage = m_SceneRenderer->GetFinalImage();
		ImGui::Image(ImGuiEx::CreateTextureRef(finalImage->GetTexture()), ImVec2(static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight)));

		UI_Toolbar();

		ImGui::End(); // Viewport
		ImGui::PopStyleVar();

		ImGui::End(); // DockSpace
	}

	auto EditorLayer::OnEvent(Event& e) -> void
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(std::bind_front(&EditorLayer::OnKeyPressed, this));
	}

	auto EditorLayer::OnKeyPressed(const KeyPressedEvent& e) -> bool
	{
		if (e.IsRepeat())
			return false;

		[[maybe_unused]] const bool alt = Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt);
		[[maybe_unused]] const bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		[[maybe_unused]] const bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		switch (e.GetKeyCode())
		{
			case Key::N:
			{
				if (control)
					m_NewProjectPopup = true;
			}

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

	auto EditorLayer::OnScenePlay() -> void
	{
		if (!m_EditorScene)
			return;

		m_SceneState = SceneState::Play;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_PanelManager->SetSceneContext(m_ActiveScene);
		m_ActiveScene->OnRuntimeStart();
	}

	auto EditorLayer::OnSceneStop() -> void
	{
		if (!m_ActiveScene)
			return;

		m_ActiveScene->OnRuntimeStop();
		m_SceneState = SceneState::Edit;
		m_ActiveScene = m_EditorScene;
		m_PanelManager->SetSceneContext(m_ActiveScene);
	}

	auto EditorLayer::CloseProject() -> void
	{
		// Unload the per-project user assembly (collectible), but keep the
		// scripting runtime + core assembly alive for the next project.
		if (ScriptEngine::IsInitialized())
			ScriptEngine::Get().UnloadUserAssembly();

		SaveProject();

		auto scene = CreateRef<Scene>();

		m_PanelManager->SetSceneContext(scene);
		
		if (Project::GetActive())
			Project::SetActive(nullptr);

		m_EditorScene = scene;
		m_ActiveScene = scene;
	}

	auto EditorLayer::NewProject(const std::string& name) -> void
	{
		// Create project directory
		const auto projectPath = FS::GetRootDirectory() / "Projects" / name;
		FS::CreateDirectory(projectPath);

	    // Create asset directories
	    FS::CreateDirectory(projectPath / "Assets" / "Meshes");
	    FS::CreateDirectory(projectPath / "Assets" / "Scenes");
	    FS::CreateDirectory(projectPath / "Assets" / "Scripts");

		// Copy new project template
		FS::Copy("Resources/Templates/NewProject", projectPath);

		// Replace tokens
		constexpr auto ReplaceToken = [](std::string& input, const char* token, const std::string& value) -> void
		{
			size_t pos = 0;
			while ((pos = input.find(token, pos)) != std::string::npos)
			{
				input.replace(pos, strlen(token), value);
				pos += strlen(token);
			}
		};

		{
			auto inputStr = FS::ReadText(projectPath / "project.epproj");
			ReplaceToken(inputStr, "$PROJECT_NAME$", name);
			FS::WriteText(projectPath / "project.epproj", inputStr, true);
			FS::Move(projectPath / "project.epproj", projectPath / std::filesystem::path(name + ".epproj"));
		}

		// Replace tokens in the scripts project and rename it after the project.
		{
			const auto templateCsproj = projectPath / "Scripts" / "Scripts.csproj";
			auto csprojStr = FS::ReadText(templateCsproj);
			ReplaceToken(csprojStr, "$APP_DIR$", FS::GetRootDirectory().string());
			FS::WriteText(templateCsproj, csprojStr, true);
			FS::Move(templateCsproj, projectPath / "Scripts" / std::filesystem::path(name + ".csproj"));
		}

		OpenProject(projectPath / std::filesystem::path(name + ".epproj"));
	}

	auto EditorLayer::OpenProject() -> bool
	{
		const auto path = FileDialog::OpenFile({
			{ "EppoEngine Project", "epproj" }
		}, FS::GetRootDirectory());

		if (path.empty())
			return false;

		return OpenProject(path);
	}

	auto EditorLayer::OpenProject(const std::filesystem::path& path) -> bool
	{
		if (path.extension().string() != ".epproj")
		{
			Log::Error("Could not load '{}' because it is not a project file!", path);
			return false;
		}

		if (Project::GetActive())
			CloseProject();

		if (Project::Open(path))
		{
			const auto& projSpec = Project::GetActive()->GetSpecification();

			// Build and load the scripting assemblies before opening the scene:
			// scene deserialization populates ScriptEngine's field storage, so
			// the engine must be initialized and the user classes known first.
			const auto scriptsProjectPath = Project::GetScriptsDirectory() / (projSpec.Name + ".csproj");
			if (FS::Exists(scriptsProjectPath))
			{
				const std::string command = std::format(
					"dotnet build \"{}\" -c Debug -o \"{}\"",
					scriptsProjectPath.string(),
					FS::GetRootDirectory().string()
				);
				std::system(command.c_str());
			}

			const auto runtimeConfigPath = FS::GetRootDirectory() / "runtimeconfig.json";
			if (ScriptEngine::Init(runtimeConfigPath))
			{
				const auto userAssemblyPath = FS::GetRootDirectory() / (projSpec.Name + ".dll");
				if (FS::Exists(userAssemblyPath))
					ScriptEngine::Get().LoadUserAssembly(userAssemblyPath);
				else
					Log::Warn("No user script assembly found at '{}'; scripts will be unavailable.", userAssemblyPath);
			}
			else
			{
				Log::Error("Failed to initialize the script runtime for project '{}'.", projSpec.Name);
			}

			// Now that scripting is ready, open the start scene.
			if (projSpec.StartScene)
				OpenScene(projSpec.StartScene);
			else
				NewScene();
		}

		return true;
	}

	auto EditorLayer::SaveProject() -> bool
	{
		SaveScene();

		if (!Project::GetActive()->GetSpecification().StartScene)
			Project::GetActive()->GetSpecification().StartScene = m_ActiveScene->Handle;

		return Project::SaveActive();
	}

	auto EditorLayer::NewScene() -> void
	{
		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
		m_ActiveScenePath = std::filesystem::path();
		m_PanelManager->SetSceneContext(m_ActiveScene);
	}

	auto EditorLayer::OpenScene() -> bool
	{
		const auto path = FileDialog::OpenFile({
			{ "EppoEngine Scene", "epscene" }
		}, FS::GetRootDirectory());

		if (path.empty())
			return false;

		return OpenScene(path);
	}

	auto EditorLayer::OpenScene(const std::filesystem::path& path) -> bool
	{
		if (path.extension().string() != ".epscene")
		{
			Log::Error("Could not load '{}' because it is not a scene file!", path);
			return false;
		}

		const auto scene = CreateRef<Scene>();
		const SceneSerializer serializer(scene);

		if (serializer.Deserialize(path))
		{
			m_EditorScene = scene;
			m_ActiveScene = m_EditorScene;
			m_ActiveScenePath = Project::GetAssetFilepath(path);
			m_PanelManager->SetSceneContext(m_ActiveScene);
		}
		else
		{
			Log::Error("Failed to deserialize scene '{}'!", path);
			return false;
		}

		return true;
	}

	auto EditorLayer::OpenScene(AssetHandle handle) -> void
	{
		const auto& assetManager = Project::GetActive()->GetAssetManager();
		m_EditorScene = std::static_pointer_cast<Scene>(assetManager->GetOrLoadAsset(handle));
		m_ActiveScene = m_EditorScene;
		m_ActiveScenePath = Project::GetAssetFilepath(assetManager->GetMetadata(handle).Filepath);

		m_PanelManager->SetSceneContext(m_ActiveScene);
	}

	auto EditorLayer::SaveScene() -> bool
	{
		bool saved = false;

		if (m_ActiveScenePath.empty())
			saved = SaveSceneAs();
		else
		{
			const SceneSerializer serializer(m_ActiveScene);
			saved = serializer.Serialize(m_ActiveScenePath);
		}

		const auto& assetManager = Project::GetActive()->GetAssetManager();
		if (assetManager && !assetManager->HasAssetData(m_ActiveScene->Handle))
			assetManager->CreateAsset(m_ActiveScenePath, m_ActiveScene);

		return saved;
	}

	auto EditorLayer::SaveSceneAs() -> bool
	{
		const auto path = FileDialog::SaveFile({
			{ "EppoEngine Scene", "epscene" }
		}, FS::GetRootDirectory());

		if (path.empty())
			return false;

		m_ActiveScenePath = path;
		const SceneSerializer serializer(m_ActiveScene);
		serializer.Serialize(m_ActiveScenePath);

		return true;
	}

	auto EditorLayer::UI_Toolbar() -> void
	{
		constexpr float toolbarWidth = 56.0f;
		constexpr float toolbarHeight = 40.0f;
		constexpr float topMargin = 24.0f;
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		ImVec2 toolbarPos = { winPos.x + (winSize.x - toolbarWidth) * 0.5f, winPos.y + topMargin };

		ImGui::SetCursorScreenPos(toolbarPos);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.35f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		constexpr auto windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::BeginChild("##Toolbar", ImVec2(toolbarWidth, toolbarHeight), ImGuiChildFlags_AlwaysUseWindowPadding, windowFlags);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.4f));

		constexpr ImVec2 buttonSize(24.0f, 24.0f);

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				if (m_PlayIcon)
				{
					if (ImGui::ImageButton("##Play", ImGuiEx::CreateTextureRef(m_PlayIcon->GetTexture()), buttonSize))
						OnScenePlay();
				}
				else if (ImGui::Button("Play", ImVec2(40.0f, 24.0f)))
				{
					OnScenePlay();
				}
				break;
			}

			case SceneState::Play:
			{
				if (m_StopIcon)
				{
					if (ImGui::ImageButton("##Stop", ImGuiEx::CreateTextureRef(m_StopIcon->GetTexture()), buttonSize))
						OnSceneStop();
				}
				else if (ImGui::Button("Stop", ImVec2(40.0f, 24.0f)))
				{
					OnSceneStop();
				}
				break;
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	auto EditorLayer::UI_NewProjectPopup() -> void
	{
		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
		if (ImGui::BeginPopupModal("New Project", nullptr, windowFlags))
		{
			static std::string projectName;

			ImGui::Text("Project Name");
			ImGui::InputText("##ProjectName", &projectName);

			const auto projectPath = FS::GetRootDirectory() / "Projects" / projectName;
			const bool projectExists = !projectName.empty() && FS::Exists(projectPath);

			if (projectExists)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
				ImGui::Text("Project name already exists!");
				ImGui::PopStyleColor();
			}
			else if (!projectName.empty())
			{
				ImGui::Text("Project path: \n%s", projectPath.string().c_str());
			}

			if (ImGui::Button("Cancel", ImVec2(100, 30)))
				ImGui::CloseCurrentPopup();

			ImGui::SameLine();

			if (projectExists)
				ImGui::BeginDisabled();

			if (ImGui::Button("Create", ImVec2(100, 30)))
			{
				NewProject(projectName);
				ImGui::CloseCurrentPopup();
			}

			if (projectExists)
				ImGui::EndDisabled();

			ImGui::EndPopup();
		}
	}
}