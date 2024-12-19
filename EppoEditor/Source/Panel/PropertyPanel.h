#pragma once

#include "Panel/Panel.h"

namespace Eppo
{
	class PropertyPanel : public Panel
	{
	public:
		explicit PropertyPanel(PanelManager& panelManager);

		void RenderGui() override;

	private:
		template<typename T>
		void DrawAddComponentEntry(const std::string& label) const;

		template<typename T, typename FN>
		void DrawComponent(Entity entity, FN uiFn, const std::string& tag = std::string());
	};
}
