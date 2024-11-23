#include "PanelManager.h"

namespace Eppo
{
	void PanelManager::Shutdown()
	{
		m_PanelData.clear();
	}

	void PanelManager::RenderGui()
	{
		for (const auto& [name, panelData] : m_PanelData)
		{
			if (panelData.IsOpen)
				panelData.Panel->RenderGui();
		}
	}

	PanelManager& PanelManager::Get()
	{
		static PanelManager p;
		return p;
	}
}
