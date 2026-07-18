#include "Panels/SceneHierarchyPanel.h"

#include <imgui.h>

namespace Eppo
{
	auto SceneHierarchyPanel::RenderGui() -> void
	{
		ScopedBegin scopedBegin("Scene Hierarchy");

		const auto& scene = GetSceneContext();

		// Tighter indent per depth level than the default.
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetStyle().IndentSpacing * 0.5f);

		// Roots only; children are drawn by recursion in DrawEntityNode.
		scene->ForEachEntity([&](Entity entity)
		{
			if (!entity.GetComponent<RelationshipComponent>().Parent)
				DrawEntityNode(entity);
		});

		ImGui::PopStyleVar();

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
			SetSelectedEntity({});

		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Create new entity"))
				scene->CreateEntity("New entity");

			ImGui::EndPopup();
		}
	}

	auto SceneHierarchyPanel::DrawEntityNode(Entity entity) -> void
	{
		const auto& scene = GetSceneContext();
		const std::string& tag = entity.GetComponent<TagComponent>().Tag;

		// Snapshot: a drag-drop reparent below can mutate the live child list.
		const std::vector<UUID> children = entity.GetComponent<RelationshipComponent>().Children;

		ImGuiTreeNodeFlags flags = (GetSelectedEntity() == entity ? ImGuiTreeNodeFlags_Selected : 0);
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (children.empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		// Seed the ImGui ID with the full 64-bit UUID (PushID(int) would truncate).
		ImGui::PushID(reinterpret_cast<const void*>(static_cast<uint64_t>(entity.GetUUID())));
		const bool opened = ImGui::TreeNodeEx(tag.c_str(), flags);

		if (ImGui::IsItemClicked())
			SetSelectedEntity(entity);

		// Drag source: the entity being moved.
		if (ImGui::BeginDragDropSource())
		{
			const uint64_t payload = static_cast<uint64_t>(entity.GetUUID());
			ImGui::SetDragDropPayload("ENTITY_UUID", &payload, sizeof(payload));
			ImGui::TextUnformatted(tag.c_str());
			ImGui::EndDragDropSource();
		}

		// Drop target: the entity dropped onto becomes the new parent.
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID"))
			{
				const UUID droppedId = *static_cast<const uint64_t*>(payload->Data);
				// SetParent ignores self/descendant drops.
				scene->SetParent(scene->GetEntityByUUID(droppedId), entity);
			}
			ImGui::EndDragDropTarget();
		}

		bool entityDeleted = false;
		bool entityUnparented = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Duplicate entity"))
				scene->DuplicateEntity(entity);
			if (ImGui::MenuItem("Unparent entity", nullptr, false, static_cast<bool>(entity.GetComponent<RelationshipComponent>().Parent)))
				entityUnparented = true;
			if (ImGui::MenuItem("Delete entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		ImGui::PopID();

		if (opened)
		{
			for (const UUID childId : children)
			{
				if (const Entity child = scene->GetEntityByUUID(childId))
					DrawEntityNode(child);
			}

			ImGui::TreePop();
		}

		if (entityUnparented)
			scene->SetParent(entity, {});

		if (entityDeleted)
		{
			if (GetSelectedEntity() == entity)
				SetSelectedEntity({});

			scene->DestroyEntity(entity);
		}
	}
}