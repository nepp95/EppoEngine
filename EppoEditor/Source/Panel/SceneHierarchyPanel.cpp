#include "SceneHierarchyPanel.h"

namespace Eppo
{
	SceneHierarchyPanel::SceneHierarchyPanel(PanelManager& panelManager)
		: Panel(panelManager)
	{}

	void SceneHierarchyPanel::RenderGui()
	{
		ImGui::Begin("Scene Hierarchy");

		GetSceneContext()->m_Registry.each([&](auto entityID)
		{
			Entity entity(entityID, GetSceneContext().get());
			DrawEntityNode(entity);
		});

		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		const std::string& tag = entity.GetComponent<TagComponent>();

		ImGuiTreeNodeFlags flags = (GetSelectedEntity() == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Bullet;

		if (ImGui::TreeNodeEx(tag.c_str(), flags))
			ImGui::TreePop();

		if (ImGui::IsItemClicked())
			SetSelectedEntity(entity); 
	}
}
