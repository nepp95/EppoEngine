#pragma once

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
		Ref<Scene> m_ActiveScene;
		EditorCamera m_EditorCamera = EditorCamera(30.0f, 1.778f);

		uint32_t m_ViewportWidth;
		uint32_t m_ViewportHeight;

		Ref<Texture> m_TestTexture;
	};
}
