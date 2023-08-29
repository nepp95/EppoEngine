#include "SceneHierarchyPanel.h"

namespace Eppo
{
	void SceneHierarchyPanel::RenderGui()
	{
		ImGui::Begin("Scene Hierarchy");

		m_SceneContext->m_Registry.each([&](auto entityID)
		{
			Entity entity(entityID, m_SceneContext.get());
			DrawEntityNode(entity);
		});

		ImGui::End();
	}

	void SceneHierarchyPanel::SetSceneContext(const Ref<Scene>& scene)
	{
		m_SceneContext = scene;
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		// TODO: How do we relay this to other panels?
		m_SelectedEntity = entity;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		const std::string& tag = entity.GetComponent<TagComponent>();

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Bullet;

		if (ImGui::TreeNodeEx(tag.c_str(), flags))
			ImGui::TreePop();

		if (ImGui::IsItemClicked())
			SetSelectedEntity(entity); 
	}
}
