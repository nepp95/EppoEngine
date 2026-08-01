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
        scene->ForEachEntity(
            [&](Entity entity)
            {
                const UUID parentId =
                    entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Parent : UUID(0);
                if (!parentId || !scene->GetEntityByUUID(parentId))
                    DrawEntityNode(entity);
            }
        );

        const ImVec2 dropTargetSize = ImGui::GetContentRegionAvail();
        // Root un-parent drop zone, shown only during an entity drag so it doesn't blanket the panel and swallow right-clicks.
        const ImGuiPayload* dragPayload = ImGui::GetDragDropPayload();
        if (dragPayload && dragPayload->IsDataType("ENTITY_UUID") && dropTargetSize.x > 0.0f && dropTargetSize.y > 0.0f)
        {
            ImGui::InvisibleButton("##HierarchyRootDropTarget", dropTargetSize);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 rectMin = ImGui::GetItemRectMin();
            const ImVec2 rectMax = ImGui::GetItemRectMax();
            const bool hovered = ImGui::BeginDragDropTarget();

            drawList->AddRect(rectMin, rectMax, IM_COL32(255, 255, 255, hovered ? 140 : 60));
            if (hovered)
            {
                drawList->AddRectFilled(rectMin, rectMax, IM_COL32(255, 255, 255, 24));
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID"))
                {
                    const UUID droppedId = *static_cast<const uint64_t*>(payload->Data);
                    scene->SetParent(scene->GetEntityByUUID(droppedId), {});
                }
                ImGui::EndDragDropTarget();
            }
        }

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
        const std::vector<UUID> children =
            entity.HasComponent<RelationshipComponent>() ? entity.GetComponent<RelationshipComponent>().Children : std::vector<UUID>{};

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
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Duplicate entity"))
                scene->DuplicateEntity(entity);
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

        if (entityDeleted)
        {
            SetSelectedEntity({});
            scene->DestroyEntity(entity);
        }
    }
}
