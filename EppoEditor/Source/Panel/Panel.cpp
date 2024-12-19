#include "Panel.h"

#include "Panel/PanelManager.h"

namespace Eppo
{
	Panel::Panel(PanelManager& panelManager)
		: m_PanelManager(panelManager)
	{}

	Ref<Scene> Panel::GetSceneContext() const
	{
		return m_PanelManager.GetSceneContext();
	}

	Entity Panel::GetSelectedEntity() const
	{
		return m_PanelManager.GetSelectedEntity();
	}

	void Panel::SetSceneContext(const Ref<Scene>& scene) const
	{
		m_PanelManager.SetSceneContext(scene);
	}

	void Panel::SetSelectedEntity(const Entity entity) const
	{
		m_PanelManager.SetSelectedEntity(entity);
	}
}
