#pragma once

#include "Panel/Panel.h"
#include "Panel/PanelManager.h"

namespace Eppo
{
	class SceneHierarchyPanel : public Panel
	{
	public:
		explicit SceneHierarchyPanel(PanelManager& panelManager);

		void RenderGui() override;

	private:
		void DrawEntityNode(Entity entity) const;
	};
}
