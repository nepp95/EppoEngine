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

		EditorCamera m_EditorCamera{ 30.0f, 1.778f };

		uint32_t m_ViewportWidth;
		uint32_t m_ViewportHeight;
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;

		PanelManager& m_PanelManager;
		Ref<Texture> m_TestTexture;
	};
}
