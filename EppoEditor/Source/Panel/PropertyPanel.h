#pragma once

#include "Panel/Panel.h"

namespace Eppo
{
	class PropertyPanel : public Panel
	{
	public:
		PropertyPanel(PanelManager& panelManager);

		void RenderGui() override;

	private:
		template<typename T, typename FN>
		void DrawComponent(Entity entity, FN uiFn, std::string& label = std::string());
	};
}
