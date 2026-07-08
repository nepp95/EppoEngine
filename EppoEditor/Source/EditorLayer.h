#pragma once

#include "Panels/PanelManager.h"

#include <EppoEngine.h>

namespace Eppo
{
	class EditorLayer : public Layer
	{
	public:
		auto OnAttach() -> void override;
		auto OnDetach() -> void override;

		auto OnUpdate(float timestep) -> void override;
		auto OnUIRender() -> void override;
		auto OnEvent(Event& e) -> void override;
		
	private:
		auto OnKeyPressed(const KeyPressedEvent& e) -> bool;

		auto OnScenePlay() -> void;
		auto OnSceneStop() -> void;
		[[nodiscard]] auto GetSelectedUUID() const -> UUID;

		auto RestoreDefaultLayout() -> void;

		auto CloseProject() -> void;
		auto NewProject(const std::string& name) -> void;
		auto OpenProject() -> bool;
		auto OpenProject(const std::filesystem::path& path) -> bool;
		auto SaveProject() -> bool;

		auto NewScene() -> void;
		auto OpenScene() -> bool;
		auto OpenScene(const std::filesystem::path& path) -> bool;
		auto OpenScene(AssetHandle handle) -> void;
		auto SaveScene() -> bool;
		auto SaveSceneAs() -> bool;

		auto UI_Toolbar() -> void;
		auto UI_NewProjectPopup() -> void;

	private:
		Ref<PanelManager> m_PanelManager = nullptr;

		Ref<Scene> m_ActiveScene = nullptr;
		Ref<Scene> m_EditorScene = nullptr;
		Ref<SceneRenderer> m_SceneRenderer = nullptr;
		std::filesystem::path m_ActiveScenePath;

		ScopedPtr<EditorCamera> m_EditorCamera = nullptr;

		Ref<Image> m_PlayIcon = nullptr;
		Ref<Image> m_StopIcon = nullptr;
		Ref<Image> m_PauseIcon = nullptr;

		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		uint32_t m_ViewportWidth = 1600;
		uint32_t m_ViewportHeight = 900;

		// Set while playing when the runtime scene has no primary camera: the play
		// view falls back to the editor camera and a non-intrusive notice is shown.
		bool m_MissingPrimaryCamera = false;

		// Restoring the docking layout must happen before any window Begin this
		// frame, so the menu item just raises this and OnUIRender services it early.
		bool m_RestoreLayoutRequested = false;

		enum class SceneState
		{
			Edit,
			Play,
		} m_SceneState = SceneState::Edit;

		// Popups
		bool m_NewProjectPopup = false;
	};

	namespace Utils
	{
		// Point-in-rounded-rect test. Used by the toolbar so clicks that land in the
		// transparent corner arcs (outside the visual rounded panel but inside the
		// rectangular widget hitbox) are ignored.
		auto IsInsideRoundedRect(const ImVec2& p, const ImVec2& min, const ImVec2& max, float radius) -> bool;
	}
}