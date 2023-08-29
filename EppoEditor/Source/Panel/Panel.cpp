#include "Panel.h"

#include "Panel/PanelManager.h"

namespace Eppo
{
	Panel::Panel(PanelManager& panelManager)
		: m_PanelManager(panelManager)
	{}

	Ref<Scene> Panel::GetSceneContext()
	{
		return m_PanelManager.GetSceneContext();
	}

	Entity Panel::GetSelectedEntity()
	{
		return m_PanelManager.GetSelectedEntity();
	}

	void Panel::SetSceneContext(const Ref<Scene>& scene)
	{
		m_PanelManager.SetSceneContext(scene);
	}

	void Panel::SetSelectedEntity(Entity entity)
	{
		m_PanelManager.SetSelectedEntity(entity);
	}
}
