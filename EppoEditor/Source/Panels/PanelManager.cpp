#include "Panels/PanelManager.h"

namespace Eppo
{
    auto PanelManager::RenderGui() -> void
    {
        for (const auto& [name, panelData] : m_PanelData)
        {
            if (panelData.IsOpen)
                panelData.Panel->RenderGui();
        }
    }

    auto PanelManager::TogglePanel(const std::string& panelName) -> void
    {
        if (const auto it = m_PanelData.find(panelName); it != m_PanelData.end())
        {
            const bool isOpen = it->second.IsOpen;
            it->second.IsOpen = !isOpen;
        }
    }
}
