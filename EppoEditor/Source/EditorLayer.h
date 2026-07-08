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
		auto RemapSelectionTo(const Ref<Scene>& scene, const UUID& uuid) -> void;

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
		Ref<Image> m_LogoIcon = nullptr;

		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		uint32_t m_ViewportWidth = 1600;
		uint32_t m_ViewportHeight = 900;

		enum class SceneState
		{
			Edit,
			Play,
		} m_SceneState = SceneState::Edit;

		// Popups
		bool m_NewProjectPopup = false;
	};
}