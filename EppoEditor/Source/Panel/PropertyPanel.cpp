#include "PropertyPanel.h"

#include <glm/gtc/type_ptr.hpp>

namespace Eppo
{
	namespace Utils
	{
		template<typename T>
		static std::string GetComponentString()
		{
			std::string fullType = typeid(T).name();
			size_t pos = fullType.find_last_of(':');
			size_t stringSize = fullType.size() - (pos + 1);

			return fullType.substr(pos + 1, stringSize - 9);
		}
	}

	PropertyPanel::PropertyPanel(PanelManager& panelManager)
		: Panel(panelManager)
	{}

	void PropertyPanel::RenderGui()
	{
		ScopedBegin scopedBegin("Properties");

		Entity entity = GetSelectedEntity();
		if (!entity)
			return;

		DrawComponent<TagComponent>(entity, [](auto& component)
		{
			std::string& tag = component.Tag;

			// ImGui wants a char*, we use std::string
			// We could cast a c_str to char*, but we still could not write to it
			char buffer[256]{ 0 };
			// Since ImGui accounts for the buffer size AND the null termination char, we can safely use strncpy.
			std::strncpy(buffer, tag.c_str(), sizeof(buffer));

			// Double quote prevents the label from showing
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				tag = std::string(buffer); // TODO: Add support for empty labels --> Don't crash...
		});

		DrawComponent<TransformComponent>(entity, [](auto& component)
		{
			if (ImGui::BeginTable("##", 4))
			{
				ImGui::TableNextColumn();
				ImGui::Text("Translation");

				ImGui::TableNextColumn();
				if (ImGui::Button("X"))
					component.Translation.x = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##TranslationX", &component.Translation.x, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Y"))
					component.Translation.y = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##TranslationY", &component.Translation.y, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Z"))
					component.Translation.z = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##TranslationZ", &component.Translation.z, 0.1f);

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Rotation");

				ImGui::TableNextColumn();
				if (ImGui::Button("X"))
					component.Rotation.x = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##RotationX", &component.Rotation.x, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Y"))
					component.Rotation.y = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##RotationY", &component.Rotation.y, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Z"))
					component.Rotation.z = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##RotationZ", &component.Rotation.z, 0.1f);

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("Scale");

				ImGui::TableNextColumn();
				if (ImGui::Button("X"))
					component.Scale.x = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##ScaleX", &component.Scale.x, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Y"))
					component.Scale.y = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##ScaleY", &component.Scale.y, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Z"))
					component.Scale.z = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##ScaleZ", &component.Scale.z, 0.1f);

				ImGui::EndTable();
			}
		});

		DrawComponent<SpriteComponent>(entity, [](auto& component)
		{
			ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path texturePath = path;
					component.Texture = texturePath.string();
					EPPO_TRACE(texturePath.string());
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
		});

		DrawComponent<ColorComponent>(entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
		});
	}

	template<typename T, typename FN>
	void PropertyPanel::DrawComponent(Entity entity, FN uiFn, std::string& label)
	{
		if (!entity.HasComponent<T>())
			return;

		if (label.empty())
			label = Utils::GetComponentString<T>();

		auto& c = entity.GetComponent<T>();
		
		if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			uiFn(c);
		}
	}
}
