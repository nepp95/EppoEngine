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

		ImGui::SameLine();

		// Add components
		if (ImGui::Button("Add component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			DrawAddComponentEntry<SpriteComponent>("Sprite");
			DrawAddComponentEntry<MeshComponent>("Mesh");
			DrawAddComponentEntry<DirectionalLightComponent>("Directional Light");
			DrawAddComponentEntry<RigidBodyComponent>("Rigid Body");
			DrawAddComponentEntry<CameraComponent>("Camera");

			ImGui::EndPopup();
		}

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
			AssetManager& assetManager = AssetManager::Get();

			if (component.TextureHandle)
			{
				ImGui::TextDisabled(assetManager.GetMetadata(component.TextureHandle).Filepath.string().c_str());
				ImGui::SameLine();
				if (ImGui::Button("X"))
					component.TextureHandle = 0;
			} else
			{
				ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_ASSET"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						std::filesystem::path texturePath = path;

						Ref<Texture> texture = assetManager.LoadAsset<Texture>(texturePath);
						component.TextureHandle = texture->Handle;
					}
					ImGui::EndDragDropTarget();
				}
			}

			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
		});

		DrawComponent<MeshComponent>(entity, [](auto& component)
		{
			AssetManager& assetManager = AssetManager::Get();

			if (component.MeshHandle)
			{
				ImGui::TextDisabled(assetManager.GetMetadata(component.MeshHandle).Filepath.string().c_str());
				ImGui::SameLine();
				if (ImGui::Button("X"))
					component.MeshHandle = 0;
			} else 
			{
				ImGui::Button("Mesh", ImVec2(100.0f, 0.0f));
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MESH_ASSET"))
					{
						const wchar_t* path = (const wchar_t*)payload->Data;
						std::filesystem::path meshPath = path;

						Ref<Mesh> mesh = assetManager.LoadAsset<Mesh>(meshPath);
						component.MeshHandle = mesh->Handle;
					}
					ImGui::EndDragDropTarget();
				}
			}
		});

		DrawComponent<DirectionalLightComponent>(entity, [](auto& component)
		{
			if (ImGui::BeginTable("##", 4))
			{
				ImGui::TableNextColumn();
				ImGui::Text("Direction");

				ImGui::TableNextColumn();
				if (ImGui::Button("X"))
					component.Direction.x = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##DirectionX", &component.Direction.x, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Y"))
					component.Direction.y = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##DirectionY", &component.Direction.y, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Z"))
					component.Direction.z = 0.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##DirectionZ", &component.Direction.z, 0.1f);

				ImGui::EndTable();
			}

			ImGui::ColorEdit4("Albedo Color", glm::value_ptr(component.AlbedoColor));
			ImGui::ColorEdit4("Ambient Color", glm::value_ptr(component.AmbientColor));
			ImGui::ColorEdit4("Specular Color", glm::value_ptr(component.SpecularColor));
		}, std::string("Directional Light"));

		DrawComponent<RigidBodyComponent>(entity, [](auto& component)
		{
			const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyType = bodyTypes[(int)component.Type];

			if (ImGui::BeginCombo("Body Type", currentBodyType))
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					bool isSelected = currentBodyType == bodyTypes[i];

					if (ImGui::Selectable(bodyTypes[i], isSelected))
					{
						currentBodyType = bodyTypes[i];
						component.Type = (RigidBodyComponent::BodyType)i;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::DragFloat("Mass", &component.Mass, 0.1f);
		}, std::string("Rigid Body"));

		DrawComponent<CameraComponent>(entity, [](auto& component)
		{
			auto& camera = component.Camera;

			const char* projectionTypes[] = { "Orthographic", "Perspective" };
			const char* currentProjectionType = projectionTypes[(int)camera.GetProjectionType()];

			if (ImGui::BeginCombo("Projection Type", currentProjectionType))
			{
				for (uint32_t i = 0; i < 2; i++)
				{
					bool isSelected = currentProjectionType == projectionTypes[i];

					if (ImGui::Selectable(projectionTypes[i], isSelected))
					{
						currentProjectionType = projectionTypes[i];
						camera.SetProjectionType((ProjectionType)i);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (camera.GetProjectionType() == ProjectionType::Perspective)
			{
				float fov = glm::degrees(camera.GetPerspectiveFov());
				if (ImGui::DragFloat("Field of view", &fov))
					camera.SetPerspectiveFov(fov);

				float nearClip = camera.GetPerspectiveNearClip();
				if (ImGui::DragFloat("Near clip", &nearClip))
					camera.SetPerspectiveNearClip(nearClip);

				float farClip = camera.GetPerspectiveFarClip();
				if (ImGui::DragFloat("Far clip", &farClip))
					camera.SetPerspectiveFarClip(farClip);
			}

			if (camera.GetProjectionType() == ProjectionType::Orthographic)
			{
				float size = camera.GetOrthographicSize();
				if (ImGui::DragFloat("Size", &size))
					camera.SetOrthographicSize(size);

				float nearClip = camera.GetOrthographicNearClip();
				if (ImGui::DragFloat("Near clip", &nearClip))
					camera.SetOrthographicNearClip(nearClip);

				float farClip = camera.GetOrthographicFarClip();
				if (ImGui::DragFloat("Far clip", &farClip))
					camera.SetOrthographicFarClip(farClip);
			}
		});
	}

	template<typename T>
	void PropertyPanel::DrawAddComponentEntry(const std::string& label)
	{
		if (!GetSelectedEntity().HasComponent<T>())
		{
			if (ImGui::MenuItem(label.c_str()))
			{
				GetSelectedEntity().AddComponent<T>();
				ImGui::CloseCurrentPopup();
			}
		}
	}

	template<typename T, typename FN>
	void PropertyPanel::DrawComponent(Entity entity, FN uiFn, const std::string& tag)
	{
		if (!entity.HasComponent<T>())
			return;

		auto& c = entity.GetComponent<T>();

		std::string label;
		if (!tag.empty())
			label = tag;
		else
			label = Utils::GetComponentString<T>();
		
		bool closedHeader = true; // If this is set to false by ImGui, we delete the component
		if (ImGui::CollapsingHeader(label.c_str(), &closedHeader, ImGuiTreeNodeFlags_DefaultOpen))
			uiFn(c);

		if (!closedHeader)
			entity.RemoveComponent<T>();
	}
}
