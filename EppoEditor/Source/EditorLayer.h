#pragma once

#include "Panels/PanelManager.h"

#include <EppoEngine.h>

#include <future>

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

		auto RestoreDefaultLayout() -> void;

		auto CloseProject() -> void;
		auto NewProject(const std::string& name) -> void;
		auto OpenProject() -> bool;
		auto OpenProject(const std::filesystem::path& path) -> bool;
		auto SaveProject() -> bool;
		auto ExportGame() -> void;

		auto NewScene() -> void;
		auto OpenScene() -> bool;
		auto OpenScene(const std::filesystem::path& path) -> bool;
		auto OpenScene(AssetHandle handle) -> void;
		auto SaveScene() -> bool;
		auto SaveSceneAs() -> bool;

	    auto UpdateImGuizmo() -> void;
		auto UI_Toolbar() -> void;
		auto UI_NewProjectPopup() -> void;
		auto UI_RelationshipRepairPopup() -> void;
		auto UI_ExportOptionsPopup() -> void;
		auto UI_ExportProgressPopup() -> void;
		auto UI_ExportResultPopup() -> void;
		auto UI_ViewportNotices() const -> void;

	private:
		Ref<PanelManager> m_PanelManager = nullptr;

		Ref<Scene> m_ActiveScene = nullptr;
		Ref<Scene> m_EditorScene = nullptr;
		Ref<SceneRenderer> m_SceneRenderer = nullptr;
		std::filesystem::path m_ActiveScenePath;

		EditorCamera m_EditorCamera;

		Ref<Image> m_PlayIcon = nullptr;
		Ref<Image> m_StopIcon = nullptr;
		Ref<Image> m_PauseIcon = nullptr;

		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		uint32_t m_ViewportWidth = 1600;
		uint32_t m_ViewportHeight = 900;

		bool m_MissingPrimaryCamera = false;
		bool m_RestoreLayoutRequested = false;

		enum class SceneState
		{
			Edit,
			Play,
		} m_SceneState = SceneState::Edit;

		Entity m_SelectedEntity;

		// Popups
		bool m_NewProjectPopup = false;
		bool m_ExportOptionsPopup = false;
		bool m_ExportProgressPopup = false;
		bool m_ExportResultPopup = false;
		std::vector<std::string> m_RelationshipRepairNotices;

	    // Export
		bool m_ExportDebug = true;
		bool m_ExportRelease = true;
		bool m_ExportInProgress = false;
		float m_ExportProgress = 0.0f;
		std::string m_ExportPhase;
		std::mutex m_ExportProgressMutex;
		std::future<ProjectExportResult> m_ExportFuture;
		ProjectExportResult m_ExportResult;

		// Gizmo
		ImGuizmo::OPERATION m_GizmoType = ImGuizmo::TRANSLATE;
	};

	namespace Utils
	{
		// Point-in-rounded-rect test. Used by the toolbar so clicks that land in the
		// transparent corner arcs (outside the visual rounded panel but inside the
		// rectangular widget hitbox) are ignored.
		auto IsInsideRoundedRect(const ImVec2& p, const ImVec2& min, const ImVec2& max, float radius) -> bool;
	}
}
