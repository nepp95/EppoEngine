#include "PanelManager.h"

namespace Eppo
{
	void PanelManager::RenderGui()
	{
		for (const auto& [name, panelData] : m_PanelData)
		{
			if (panelData.IsOpen)
				panelData.Panel->RenderGui();
		}
	}

	void PanelManager::SetSceneContext(const Ref<Scene>& scene)
	{
		for (const auto& [name, panelData] : m_PanelData)
			panelData.Panel->SetSceneContext(scene);
	}

	void PanelManager::SetSelectedEntity(Entity entity)
	{
		for (const auto& [name, panelData] : m_PanelData)
			panelData.Panel->SetSelectedEntity(entity);
	}
}
