#pragma once

#include <EppoEngine.h>

namespace Eppo
{
	class PanelManager;

	class Panel : public RefCounter
	{
	public:
		Panel(PanelManager& panelManager);

		virtual ~Panel() = default;

		virtual void RenderGui() = 0;

	protected:
		Ref<Scene> GetSceneContext();
		Entity GetSelectedEntity();
		
		void SetSceneContext(const Ref<Scene>& scene);
		void SetSelectedEntity(Entity entity);

	protected:
		PanelManager& m_PanelManager;
	};
}
