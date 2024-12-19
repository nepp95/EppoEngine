#pragma once

#include <EppoEngine.h>

namespace Eppo
{
	class PanelManager;

	class Panel
	{
	public:
		explicit Panel(PanelManager& panelManager);
		virtual ~Panel() = default;

		virtual void RenderGui() = 0;

	protected:
		[[nodiscard]] Ref<Scene> GetSceneContext() const;
		[[nodiscard]] Entity GetSelectedEntity() const;
		
		void SetSceneContext(const Ref<Scene>& scene) const;
		void SetSelectedEntity(Entity entity) const;

	protected:
		PanelManager& m_PanelManager;
	};
}
