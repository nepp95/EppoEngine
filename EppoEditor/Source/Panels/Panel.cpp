#include "Panels/Panel.h"

#include "Panels/PanelManager.h"

namespace Eppo
{
    auto Panel::SetPanelManager(PanelManager* panelManager) -> void
    {
        m_PanelManager = panelManager;
    }

    auto Panel::GetSceneContext() const -> Ref<Scene>
    {
        return m_PanelManager->GetSceneContext();
    }

    auto Panel::SetSelectedEntity(Entity entity) -> void
    {
        m_PanelManager->SetSelectedEntity(entity);
    }

    auto Panel::GetSelectedEntity() const -> Entity
    {
        return m_PanelManager->GetSelectedEntity();
    }
}
