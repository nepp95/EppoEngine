#pragma once

#include "Panel/PanelManager.h"

#include <EppoEngine.h>

namespace Eppo
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		~EditorLayer() override = default;
	
		void OnAttach() override;
		void OnDetach() override;
	
		void Update(float timestep) override;
		void Render() override;
		void RenderGui() override;

		void OnEvent(Event& e) override;

	private:
		bool OnKeyPressed(const KeyPressedEvent& e);

		void OnScenePlay();
		void OnSceneStop();

		void CloseProject();
		void NewProject(const std::string& name);
		bool OpenProject();
		void OpenProject(const std::filesystem::path& filepath);
		void SaveProject();

		void NewScene();
		void OpenScene(AssetHandle handle);
		void OpenScene(const std::filesystem::path& filepath);
		void SaveScene();
		void SaveSceneAs();

		void ImportAsset();

		void UI_File_NewProject();
		void UI_File_Preferences();
		void UI_Toolbar();

	private:
		// Scene
		Ref<SceneRenderer> m_ViewportRenderer;
		Ref<Scene> m_ActiveScene = CreateRef<Scene>();
		Ref<Scene> m_EditorScene = CreateRef<Scene>();
		std::filesystem::path m_ActiveScenePath;
		
		// Editor
		PanelManager& m_PanelManager;
		EditorCamera m_EditorCamera = EditorCamera(glm::vec3(-10.0f, 1.0f, 0.0f), 0.0f, 0.0f);

		// Viewport
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;

		// Scene state
		enum class SceneState : uint8_t
		{
			Edit,
			Play
		};
		SceneState m_SceneState = SceneState::Edit;

		// Resources
		Ref<Image> m_IconPlay;
		Ref<Image> m_IconStop;
	};
}
