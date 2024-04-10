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
		bool OnKeyPressed(KeyPressedEvent& e);

		void OnScenePlay();
		void OnSceneStop();

		void NewScene();
		void SaveScene();
		void SaveScene(const std::filesystem::path& filepath);
		void OpenScene();
		void OpenScene(const std::filesystem::path& filepath);

	private:
		// Scene
		Ref<SceneRenderer> m_ViewportRenderer;
		Ref<Scene> m_ActiveScene;
		std::filesystem::path m_ActiveScenePath;
		
		// Editor
		PanelManager& m_PanelManager;
		EditorCamera m_EditorCamera;

		// Viewport
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;

		// Scene state
		enum class SceneState
		{
			Edit,
			Play
		};
		SceneState m_SceneState = SceneState::Edit;

		// Resources
		Ref<Texture> m_IconPlay;
		Ref<Texture> m_IconStop;
	};
}
