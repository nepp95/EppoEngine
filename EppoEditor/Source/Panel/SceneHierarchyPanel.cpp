#include "SceneHierarchyPanel.h"

namespace Eppo
{
	SceneHierarchyPanel::SceneHierarchyPanel(PanelManager& panelManager)
		: Panel(panelManager)
	{}

	void SceneHierarchyPanel::RenderGui()
	{
		ScopedBegin scopedBegin("Scene Hierarchy");

		GetSceneContext()->m_Registry.each([&](auto entityID)
		{
			Entity entity(entityID, GetSceneContext().get());
			DrawEntityNode(entity);
		});

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
			SetSelectedEntity({});

		if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Create new entity"))
				GetSceneContext()->CreateEntity("New entity");

			ImGui::EndPopup();
		}
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		const std::string& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = (GetSelectedEntity() == entity ? ImGuiTreeNodeFlags_Selected : 0);
		flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

		ImGui::PushID(reinterpret_cast<void*>(static_cast<uint64_t>(entity.GetUUID())));
		const bool opened = ImGui::TreeNodeEx(tag.c_str(), flags);
		
		if (ImGui::IsItemClicked())
			SetSelectedEntity(entity);

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Duplicate entity"))
				EPPO_ASSERT(false);
			if (ImGui::MenuItem("Delete entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (opened)
			ImGui::TreePop();

		if (entityDeleted)
		{
			if (GetSelectedEntity() == entity)
				SetSelectedEntity({});

			GetSceneContext()->DestroyEntity(entity);
		}
	}
}
