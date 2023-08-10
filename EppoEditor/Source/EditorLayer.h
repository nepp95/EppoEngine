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

		void OnEvent(Event& e) override;
	
	private:
		Ref<Scene> m_ActiveScene;

		Ref<Texture> m_TestTexture;
	};
}
