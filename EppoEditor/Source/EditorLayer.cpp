#include "EditorLayer.h"

#include "Renderer/Image.h"

#include <glm/gtc/type_ptr.hpp>

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
		m_PauseIcon = loadIcon("PauseButton.png");

		// Use the pictorial logo as the OS window/taskbar icon (best-effort). It is
		// intentionally not drawn in the menu bar; the wordmark is the in-app brand
		const auto logoPath = FS::GetResourcesDirectory() / "Icons" / "Logo.png";
		if (FS::Exists(logoPath))
			Application::Get().GetWindow()->SetIcon(logoPath);

		if (!OpenProject())
		{
			m_ActiveScene = CreateRef<Scene>();
			m_PanelManager->SetSceneContext(m_ActiveScene);
		}

		m_SceneRenderer = CreateRef<SceneRenderer>(m_ActiveScene, m_ViewportWidth, m_ViewportHeight);

		// Debug overlays (selection wireframe, collider shapes) draw into the scene's
		// geometry framebuffer, so it is shared with the DebugRenderer.
		m_DebugRenderer = CreateRef<DebugRenderer>(m_ViewportWidth, m_ViewportHeight, m_SceneRenderer->GetGeometryFramebuffer());
	}

	auto EditorLayer::OnDetach() -> void
	{
		ScriptEngine::Shutdown();
		Project::SetActive(nullptr);
	}

	auto EditorLayer::OnUpdate(float timestep) -> void
	{
	    EP_PROFILE_FN("EditorLayer::OnUpdate");

		// Sync the selected entity from the panel manager before any rendering code
		// reads it. The panel manager's selection is set during the previous frame's
		// UI pass (when the user clicks in the hierarchy), so this is always one
		// frame behind — intentional and invisible for wireframe / gizmo feedback.
		m_SelectedEntity = m_PanelManager->GetSelectedEntity();

		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
		{
			m_EditorCamera->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_ActiveScene->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_EditorScene->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
			m_SceneRenderer->Resize(m_ViewportWidth, m_ViewportHeight);
		}

		// Gate polled gameplay input (editor camera + running scripts) on the viewport
		// being focused. Anything that reads Input::IsKeyPressed/IsMouseButtonPressed
		// then stays quiet while another panel is active, so e.g. typing an entity's
		// name never drives the scene. m_ViewportFocused is from last frame's UI pass,
		// which is close enough and avoids a one-frame input spill.
		Input::SetViewportInputEnabled(m_ViewportFocused);

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_MissingPrimaryCamera = false;

				if (m_ViewportFocused && !ImGuizmo::IsUsing())
					m_EditorCamera->OnUpdate(timestep);

				m_ActiveScene->OnRenderEditor(m_SceneRenderer, m_EditorCamera);
				break;
			}

			case SceneState::Play:
			{
				m_ActiveScene->OnUpdateRuntime(timestep);

				// The runtime view renders through the scene's primary camera. Without
				// one, OnRenderRuntime would draw nothing and leave a stale frame,
				// making live component edits look ignored. Fall back to the editor
				// camera so the scene (and edits) stay visible, and flag a notice.
				if (m_ActiveScene->GetPrimaryCameraEntity())
				{
					m_MissingPrimaryCamera = false;
					m_ActiveScene->OnRenderRuntime(m_SceneRenderer);
				}
				else
				{
					m_MissingPrimaryCamera = true;
					m_ActiveScene->OnRenderEditor(m_SceneRenderer, m_EditorCamera);
				}
				break;
			}
		}

		RenderDebugOverlays();
	}

	auto EditorLayer::RenderDebugOverlays() -> void
	{
		EP_PROFILE_FN("EditorLayer::RenderDebugOverlays");

		// Pick the camera the scene was rendered with so debug overlays line up.
		glm::mat4 view;
		glm::mat4 projection;
		glm::vec3 position;
		if (m_SceneState == SceneState::Play && m_ActiveScene->GetPrimaryCameraEntity())
		{
			const auto& camEntity = m_ActiveScene->GetPrimaryCameraEntity();
			const auto& cc = camEntity.GetComponent<CameraComponent>();
			const auto& tc = camEntity.GetComponent<TransformComponent>();
			const glm::mat4 camTransform = glm::translate(glm::mat4(1.0f), tc.Translation) * glm::mat4_cast(glm::quat(tc.Rotation));
			view = glm::inverse(camTransform);
			projection = cc.Camera.GetProjectionMatrix();
			position = tc.Translation;
		}
		else
		{
			view = m_EditorCamera->GetViewMatrix();
			projection = m_EditorCamera->GetProjectionMatrix();
			position = m_EditorCamera->GetPosition();
		}

		m_DebugRenderer->Begin();
		m_DebugRenderer->SetCamera(view, projection, position);

		// Selected-entity highlight (editor mode only, matching the old behaviour).
		if (m_SceneState == SceneState::Edit && m_SelectedEntity && m_SelectedEntity.HasComponent<MeshComponent>())
		{
			const auto& mc = m_SelectedEntity.GetComponent<MeshComponent>();
			if (mc.MeshHandle)
			{
				const auto& mesh = Project::GetActive()->GetAssetManager()->GetOrLoadAsset<Mesh>(mc.MeshHandle);
				m_DebugRenderer->DrawMesh(mesh, m_ActiveScene->GetWorldTransform(m_SelectedEntity), glm::vec4(0.91f, 0.39f, 0.11f, 1.0f));
			}
		}

		// Collider wireframes for every entity that has a collider component.
		if (m_ShowColliders)
		{
			m_ActiveScene->ForEachEntity([this](Entity entity)
			{
				const glm::mat4 world = m_ActiveScene->GetWorldTransform(entity);

				if (entity.HasComponent<BoxColliderComponent>())
				{
					const auto& c = entity.GetComponent<BoxColliderComponent>();
					m_DebugRenderer->DrawBox(glm::translate(world, c.Offset), c.HalfExtents, glm::vec4(0.2f, 0.8f, 0.3f, 1.0f));
				}
				else if (entity.HasComponent<SphereColliderComponent>())
				{
					const auto& c = entity.GetComponent<SphereColliderComponent>();
					m_DebugRenderer->DrawSphere(glm::translate(world, c.Offset), c.Radius, glm::vec4(0.2f, 0.8f, 0.3f, 1.0f));
				}
				else if (entity.HasComponent<CapsuleColliderComponent>())
				{
					const auto& c = entity.GetComponent<CapsuleColliderComponent>();
					m_DebugRenderer->DrawCapsule(glm::translate(world, c.Offset), c.Radius, c.Height, glm::vec4(0.2f, 0.8f, 0.3f, 1.0f));
				}
			});
		}

		const auto& dm = DeviceManager::Get();
		m_DebugRenderer->Render(dm->GetCurrentBackBufferIndex());
	}

	auto EditorLayer::OnUIRender() -> void
	{
	    EP_PROFILE_FN("EditorLayer::OnUIRender");

		// Apply a requested layout restore before any window Begin this frame, so the
		// docked windows pick up the restored dock nodes as they are submitted below.
		// Doing this mid-frame (from the menu handler) leaves windows already placed
		// and corrupts the docking layout, especially with multi-viewport enabled.
		if (m_RestoreLayoutRequested)
		{
			RestoreDefaultLayout();
			m_RestoreLayoutRequested = false;
		}

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
			// Branding: an accent-colored wordmark, vertically centered in the bar,
			// then a subtle divider before the menus. AlignTextToFramePadding lines
			// the text up with the framed menu labels instead of top-aligning it.
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(ImVec4(0.91f, 0.39f, 0.11f, 1.0f), "EppoEditor");
			ImGui::SameLine(0.0f, 12.0f);

			const ImVec2 dividerPos = ImGui::GetCursorScreenPos();
			const float dividerHeight = ImGui::GetFrameHeight();
			ImGui::GetWindowDrawList()->AddLine(
				{ dividerPos.x, dividerPos.y + 4.0f },
				{ dividerPos.x, dividerPos.y + dividerHeight - 4.0f },
				ImGui::GetColorU32(ImGuiCol_Separator));
			ImGui::SameLine(0.0f, 12.0f);

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
				if (ImGui::MenuItem("Content Browser", nullptr, m_PanelManager->IsPanelOpen(CONTENT_BROWSER_PANEL)))
					m_PanelManager->TogglePanel(CONTENT_BROWSER_PANEL);

				if (ImGui::MenuItem("Properties", nullptr, m_PanelManager->IsPanelOpen(PROPERTY_PANEL)))
					m_PanelManager->TogglePanel(PROPERTY_PANEL);

				if (ImGui::MenuItem("Scene Hierarchy", nullptr, m_PanelManager->IsPanelOpen(SCENE_HIERARCHY_PANEL)))
					m_PanelManager->TogglePanel(SCENE_HIERARCHY_PANEL);

				ImGui::Separator();

				if (ImGui::MenuItem("Restore window layout"))
					m_RestoreLayoutRequested = true;

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Show Colliders", nullptr, m_ShowColliders))
					m_ShowColliders = !m_ShowColliders;

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

    // Popups
	UI_NewProjectPopup();

	// Scene render
	m_SceneRenderer->RenderGui();
		m_DebugRenderer->RenderGui(DeviceManager::Get()->GetCurrentBackBufferIndex());

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

	    // Imguizmo
	    UpdateImGuizmo();

	    // UI
		UI_Toolbar();
        UI_WarningNoPrimaryCamera();

		ImGui::End(); // Viewport
		ImGui::PopStyleVar();

		ImGui::End(); // DockSpace
	}

	auto EditorLayer::OnEvent(Event& e) -> void
	{
	    EP_PROFILE_FN("EditorLayer::OnEvent");

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(std::bind_front(&EditorLayer::OnKeyPressed, this));
	}

	auto EditorLayer::OnKeyPressed(const KeyPressedEvent& e) -> bool
	{
	    EP_PROFILE_FN("EditorLayer::OnKeyPressed");

		if (e.IsRepeat())
			return false;

		// Read modifiers ungated: editor accelerators must work regardless of whether
		// the viewport currently owns gameplay input.
		[[maybe_unused]] const bool alt = Input::IsKeyPressedRaw(Key::LeftAlt) || Input::IsKeyPressedRaw(Key::RightAlt);
		[[maybe_unused]] const bool control = Input::IsKeyPressedRaw(Key::LeftControl) || Input::IsKeyPressedRaw(Key::RightControl);
		[[maybe_unused]] const bool shift = Input::IsKeyPressedRaw(Key::LeftShift) || Input::IsKeyPressedRaw(Key::RightShift);

		switch (e.GetKeyCode())
		{
		    case Key::N:
		    {
			    if (control)
				    m_NewProjectPopup = true;
			    break;
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

			case Key::W:
			{
				if (!alt && !control && !shift && m_SceneState == SceneState::Edit)
					m_GizmoType = ImGuizmo::TRANSLATE;
				break;
			}

			case Key::E:
			{
				if (!alt && !control && !shift && m_SceneState == SceneState::Edit)
					m_GizmoType = ImGuizmo::ROTATE;
				break;
			}

			case Key::R:
			{
				if (!alt && !control && !shift && m_SceneState == SceneState::Edit)
					m_GizmoType = ImGuizmo::SCALE;
				break;
			}
		}

		return false;
	}

	auto EditorLayer::OnScenePlay() -> void
	{
	    EP_PROFILE_FN("EditorLayer::OnScenePlay");

		if (!m_EditorScene)
			return;

		// Capture the selection's UUID before switching scenes; the handle is only
		// valid in the editor registry, but the UUID survives Scene::Copy.
		const UUID selectedUUID = GetSelectedUUID();

		m_SceneState = SceneState::Play;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_PanelManager->SetSceneContext(m_ActiveScene);

		// Re-resolve the selection by UUID in the runtime copy so the Property panel
		// edits the entity that is actually being rendered (handles don't survive the
		// copy, UUIDs do). Clear it if the UUID isn't present.
		m_SelectedEntity = selectedUUID ? m_ActiveScene->GetEntityByUUID(selectedUUID) : Entity{};
		m_PanelManager->SetSelectedEntity(m_SelectedEntity);

		m_ActiveScene->OnRuntimeStart();
	}

	auto EditorLayer::OnSceneStop() -> void
	{
	    EP_PROFILE_FN("EditorLayer::OnSceneStop");

		if (!m_ActiveScene)
			return;

		m_ActiveScene->OnRuntimeStop();

		// Read the selection's UUID while the runtime scene it points at is still
		// alive (it is dropped by the assignment below).
		const UUID selectedUUID = GetSelectedUUID();

		m_SceneState = SceneState::Edit;
		m_ActiveScene = m_EditorScene;
		m_PanelManager->SetSceneContext(m_ActiveScene);

		// Map the selection back onto the editor scene by UUID (see OnScenePlay).
		m_SelectedEntity = selectedUUID ? m_ActiveScene->GetEntityByUUID(selectedUUID) : Entity{};
		m_PanelManager->SetSelectedEntity(m_SelectedEntity);
	}

	auto EditorLayer::GetSelectedUUID() const -> UUID
	{
		const Entity selected = m_PanelManager->GetSelectedEntity();
		return selected ? selected.GetUUID() : UUID(0);
	}

	auto EditorLayer::RestoreDefaultLayout() -> void
	{
		const auto path = FS::GetResourcesDirectory() / "Layouts" / "DefaultLayout.ini";
		if (!FS::Exists(path))
		{
			Log::Error("Cannot restore window layout: default layout not found at '{}'", path);
			return;
		}

		const std::string layout = FS::ReadText(path);
		if (layout.empty())
			return; // FS::ReadText already logged the failure.

		ImGui::LoadIniSettingsFromMemory(layout.c_str(), layout.size());

		// The docking layout references panel windows by name, so reopen every panel
		// to guarantee the restored dock nodes have their windows to populate.
		m_PanelManager->SetPanelOpen(CONTENT_BROWSER_PANEL, true);
		m_PanelManager->SetPanelOpen(PROPERTY_PANEL, true);
		m_PanelManager->SetPanelOpen(SCENE_HIERARCHY_PANEL, true);
	}

	auto EditorLayer::CloseProject() -> void
	{
	    EP_PROFILE_FN("EditorLayer::CloseProject");

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
	    EP_PROFILE_FN("EditorLayer::NewProject");

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
	    EP_PROFILE_FN("EditorLayer::OpenProject");

		const auto path = FileDialog::OpenFile({
			{ "EppoEngine Project", "epproj" }
		}, FS::GetRootDirectory());

		if (path.empty())
			return false;

		return OpenProject(path);
	}

	auto EditorLayer::OpenProject(const std::filesystem::path& path) -> bool
	{
	    EP_PROFILE_FN("EditorLayer::OpenProject");

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
	    EP_PROFILE_FN("EditorLayer::SaveProject");

		SaveScene();

		if (!Project::GetActive()->GetSpecification().StartScene)
			Project::GetActive()->GetSpecification().StartScene = m_ActiveScene->Handle;

		return Project::SaveActive();
	}

	auto EditorLayer::NewScene() -> void
	{
	    EP_PROFILE_FN("EditorLayer::NewScene");

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
		m_ActiveScenePath = std::filesystem::path();
		m_PanelManager->SetSceneContext(m_ActiveScene);
	}

	auto EditorLayer::OpenScene() -> bool
	{
	    EP_PROFILE_FN("EditorLayer::OpenScene");

		const auto path = FileDialog::OpenFile({
			{ "EppoEngine Scene", "epscene" }
		}, FS::GetRootDirectory());

		if (path.empty())
			return false;

		return OpenScene(path);
	}

	auto EditorLayer::OpenScene(const std::filesystem::path& path) -> bool
	{
	    EP_PROFILE_FN("EditorLayer::OpenScene");

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
	    EP_PROFILE_FN("EditorLayer::OpenScene");

		const auto& assetManager = Project::GetActive()->GetAssetManager();
		m_EditorScene = std::static_pointer_cast<Scene>(assetManager->GetOrLoadAsset(handle));
		m_ActiveScene = m_EditorScene;
		m_ActiveScenePath = Project::GetAssetFilepath(assetManager->GetMetadata(handle).Filepath);

		m_PanelManager->SetSceneContext(m_ActiveScene);
	}

	auto EditorLayer::SaveScene() -> bool
	{
	    EP_PROFILE_FN("EditorLayer::SaveScene");

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
	    EP_PROFILE_FN("EditorLayer::SaveSceneAs");

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

    auto EditorLayer::UpdateImGuizmo() -> void
    {
	    if (m_SceneState != SceneState::Edit)
			return;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);

        // Camera matrices — use the same unflipped projection the renderer uses
        // (NVRHI flips Y at the viewport level for Vulkan; ImGuizmo expects Y-up
        // clip space and maps it to screen coordinates internally).
        auto& camera = *m_EditorCamera;
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 proj = camera.GetProjectionMatrix();

        // Entity transform gizmo
        if (m_SelectedEntity && m_SelectedEntity.HasComponent<TransformComponent>())
        {
            auto& tc = m_SelectedEntity.GetComponent<TransformComponent>();
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.Translation)
                * glm::mat4_cast(glm::quat(tc.Rotation))
                * glm::scale(glm::mat4(1.0f), tc.Scale);

            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, nullptr);

            if (ImGuizmo::IsUsing())
            {
                glm::vec3 translation, rotation, scale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));

                tc.Translation = translation;
                tc.Rotation = glm::radians(rotation);
                tc.Scale = scale;
            }
        }

        // View orientation indicator in the top-right corner (display-only —
        // interactivity would require syncing back into EditorCamera's internal
        // position/pitch/yaw, adding fragility for little gain).
        ImGuizmo::ViewManipulate(glm::value_ptr(view), 8.0f,
            ImVec2(imageMax.x - 128.0f, imageMin.y),
            ImVec2(128.0f, 128.0f),
            0x00000000);
    }

    auto EditorLayer::UI_Toolbar() -> void
	{
		constexpr float buttonSize = 30.0f;
		constexpr float rounding = 8.0f;
		constexpr float topMargin = 24.0f;

		enum class ToolbarAction { None, Play, Stop };
		struct ToolbarButton
		{
			const char* Id;
			Ref<Image> Icon;
			const char* Fallback;
			bool Enabled;
			ToolbarAction Action;
		};

		// Buttons are packed edge-to-edge (no padding/spacing); the panel supplies
		// the rounded corners. Pause is shown during play but intentionally not
		// wired up yet, so it renders disabled.
		std::vector<ToolbarButton> buttons;
		switch (m_SceneState)
		{
			case SceneState::Edit:
				buttons.push_back({ "##Play", m_PlayIcon, "Play", true, ToolbarAction::Play });
				break;
			case SceneState::Play:
				buttons.push_back({ "##Pause", m_PauseIcon, "II", false, ToolbarAction::None });
				buttons.push_back({ "##Stop", m_StopIcon, "Stop", true, ToolbarAction::Stop });
				break;
		}

		if (buttons.empty())
			return;

		const float panelWidth = buttonSize * static_cast<float>(buttons.size());
		const ImVec2 winPos = ImGui::GetWindowPos();
		const ImVec2 winSize = ImGui::GetWindowSize();
		const ImVec2 panelMin = { winPos.x + (winSize.x - panelWidth) * 0.5f, winPos.y + topMargin };
		const ImVec2 panelMax = { panelMin.x + panelWidth, panelMin.y + buttonSize };

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(panelMin, panelMax, ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.10f, 0.85f)), rounding);

		for (size_t i = 0; i < buttons.size(); i++)
		{
			const ToolbarButton& button = buttons[i];

			const ImVec2 p0 = { panelMin.x + buttonSize * static_cast<float>(i), panelMin.y };
			const ImVec2 p1 = { p0.x + buttonSize, p0.y + buttonSize };

			// Round only the corners this button shares with the panel.
			ImDrawFlags corners = ImDrawFlags_RoundCornersNone;
			if (i == 0)
				corners |= ImDrawFlags_RoundCornersLeft;
			if (i == buttons.size() - 1)
				corners |= ImDrawFlags_RoundCornersRight;

			ImGui::SetCursorScreenPos(p0);
			ImGui::InvisibleButton(button.Id, ImVec2(buttonSize, buttonSize));

			// The hitbox is rectangular; ignore hovers/clicks in the rounded corner
			// arcs so the outer, non-button region doesn't activate.
			const bool inside = Utils::IsInsideRoundedRect(ImGui::GetIO().MousePos, panelMin, panelMax, rounding);
			const bool hovered = button.Enabled && inside && ImGui::IsItemHovered();
			const bool held = hovered && ImGui::IsItemActive();
			const bool clicked = button.Enabled && inside && ImGui::IsItemClicked();

			if (held)
				drawList->AddRectFilled(p0, p1, ImGui::GetColorU32(ImVec4(0.91f, 0.39f, 0.11f, 0.90f)), rounding, corners);
			else if (hovered)
				drawList->AddRectFilled(p0, p1, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.14f)), rounding, corners);

			const ImU32 tint = button.Enabled ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 70);
			if (button.Icon)
			{
				constexpr float pad = 6.0f;
				drawList->AddImage(ImGuiEx::CreateTextureRef(button.Icon->GetTexture()),
					{ p0.x + pad, p0.y + pad }, { p1.x - pad, p1.y - pad }, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
			}
			else
			{
				const ImVec2 ts = ImGui::CalcTextSize(button.Fallback);
				drawList->AddText({ p0.x + (buttonSize - ts.x) * 0.5f, p0.y + (buttonSize - ts.y) * 0.5f }, tint, button.Fallback);
			}

			if (clicked)
			{
				switch (button.Action)
				{
					case ToolbarAction::Play: OnScenePlay(); break;
					case ToolbarAction::Stop: OnSceneStop(); break;
					case ToolbarAction::None: break;
				}
				break; // scene state changed; stop iterating this frame's snapshot
			}
		}
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

    auto EditorLayer::UI_WarningNoPrimaryCamera() -> void
    {
	    // Non-intrusive notice: while playing without a primary camera, the viewport
	    // shows the editor camera's view instead of the game view. A small pill in the
	    // corner explains why, so live edits still being applied don't look broken.
	    if (m_SceneState == SceneState::Play && m_MissingPrimaryCamera)
	    {
	        constexpr const char* notice = "No primary camera - showing editor view";
	        ImDrawList* drawList = ImGui::GetWindowDrawList();
	        const ImVec2 imageMin = ImGui::GetItemRectMin();
	        constexpr ImVec2 pad = { 8.0f, 5.0f };
	        const ImVec2 textPos = { imageMin.x + 10.0f, imageMin.y + 10.0f };
	        const ImVec2 textSize = ImGui::CalcTextSize(notice);
	        drawList->AddRectFilled(
                { textPos.x - pad.x, textPos.y - pad.y },
                { textPos.x + textSize.x + pad.x, textPos.y + textSize.y + pad.y },
                IM_COL32(18, 18, 20, 205), 4.0f);
	        drawList->AddText(textPos, IM_COL32(232, 150, 60, 255), notice);
	    }
    }

    namespace Utils
	{
		// Point-in-rounded-rect test. Used by the toolbar so clicks that land in the
		// transparent corner arcs (outside the visual rounded panel but inside the
		// rectangular widget hitbox) are ignored.
		auto IsInsideRoundedRect(const ImVec2& p, const ImVec2& min, const ImVec2& max, float radius) -> bool
		{
			if (p.x < min.x || p.x > max.x || p.y < min.y || p.y > max.y)
				return false;

			const auto outsideCorner = [&](float cx, float cy) -> bool
			{
				const float dx = p.x - cx;
				const float dy = p.y - cy;
				return dx * dx + dy * dy > radius * radius;
			};

			if (p.x < min.x + radius && p.y < min.y + radius) // top-left
				return !outsideCorner(min.x + radius, min.y + radius);
			if (p.x > max.x - radius && p.y < min.y + radius) // top-right
				return !outsideCorner(max.x - radius, min.y + radius);
			if (p.x < min.x + radius && p.y > max.y - radius) // bottom-left
				return !outsideCorner(min.x + radius, max.y - radius);
			if (p.x > max.x - radius && p.y > max.y - radius) // bottom-right
				return !outsideCorner(max.x - radius, max.y - radius);

			return true;
		}
	}
}