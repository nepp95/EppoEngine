#include "Panels/PropertyPanel.h"

#include <imgui_stdlib.h>

namespace Eppo
{
	namespace Utils
	{
		template<typename T>
		static auto GetComponentString() -> std::string
		{
			const std::string fullType = typeid(T).name();
			const size_t pos = fullType.find_last_of(':');
			const size_t stringSize = fullType.size() - (pos + 1);

			return fullType.substr(pos + 1, stringSize - 9);
		}

		// Renders an editor widget for a single script field, mutating the value
		// buffer in place. ImGui reads/writes the typed value directly. Returns
		// true on the frames the user changes the value, so the caller can push
		// the edit to a live script instance during play.
		static auto DrawScriptField(const EppoScriptCore::ScriptField& field, ScriptFieldValue& value) -> bool
		{
			using FT = EppoScriptCore::ScriptFieldType;

			const std::string label = "##" + field.Name;
			void* data = value.Buffer.data();

			ImGui::TableNextColumn();
			ImGui::Text("%s", field.Name.c_str());
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);

			switch (field.Type)
			{
				case FT::Float:   return ImGui::DragScalar(label.c_str(), ImGuiDataType_Float, data, 0.1f);
				case FT::Double:  return ImGui::InputScalar(label.c_str(), ImGuiDataType_Double, data);
				case FT::Bool:    return ImGui::Checkbox(label.c_str(), reinterpret_cast<bool*>(data));
				case FT::Char:    return ImGui::InputScalar(label.c_str(), ImGuiDataType_U16, data);
				case FT::Int16:   return ImGui::InputScalar(label.c_str(), ImGuiDataType_S16, data);
				case FT::Int32:   return ImGui::DragScalar(label.c_str(), ImGuiDataType_S32, data);
				case FT::Int64:   return ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, data);
				case FT::Byte:    return ImGui::InputScalar(label.c_str(), ImGuiDataType_U8, data);
				case FT::UInt16:  return ImGui::InputScalar(label.c_str(), ImGuiDataType_U16, data);
				case FT::UInt32:  return ImGui::InputScalar(label.c_str(), ImGuiDataType_U32, data);
				case FT::UInt64:  return ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, data);
				case FT::Vector2: return ImGui::DragScalarN(label.c_str(), ImGuiDataType_Float, data, 2, 0.1f);
				case FT::Vector3: return ImGui::DragScalarN(label.c_str(), ImGuiDataType_Float, data, 3, 0.1f);
				case FT::Vector4: return ImGui::DragScalarN(label.c_str(), ImGuiDataType_Float, data, 4, 0.1f);
				case FT::Entity:  return ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, data);
				default:          ImGui::TextDisabled("(unsupported type)"); return false;
			}
		}
	}

	auto PropertyPanel::RenderGui() -> void
	{
		ScopedBegin scopedBegin("Properties");

		Entity entity = GetSelectedEntity();
		if (!entity)
			return;

		DrawComponent<TagComponent>(entity, [](auto& component)
		{
			ImGui::InputText("##Tag", &component.Tag);
		});

		ImGui::SameLine();
		
		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			DrawAddComponentEntry<MeshComponent>("Mesh");
			DrawAddComponentEntry<CameraComponent>("Camera");
			DrawAddComponentEntry<ScriptComponent>("Script");

			ImGui::EndPopup();
		}

		DrawComponent<TransformComponent>(entity, [](auto& component)
		{
			if (ImGui::BeginTable("##", 4))
			{
				ImGui::PushID("Translation");
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
				ImGui::PopID();

				ImGui::PushID("Rotation");
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
				ImGui::PopID();

				ImGui::PushID("Scale");
				ImGui::TableNextColumn();
				ImGui::Text("Scale");

				ImGui::TableNextColumn();
				if (ImGui::Button("X"))
					component.Scale.x = 1.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##ScaleX", &component.Scale.x, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Y"))
					component.Scale.y = 1.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##ScaleY", &component.Scale.y, 0.1f);

				ImGui::TableNextColumn();
				if (ImGui::Button("Z"))
					component.Scale.z = 1.0f;
				ImGui::SameLine();
				ImGui::DragFloat("##ScaleZ", &component.Scale.z, 0.1f);

				ImGui::PopID();
				ImGui::EndTable();
			}
		});

		DrawComponent<MeshComponent>(entity, [](auto& component)
		{
			if (component.MeshHandle)
			{
				const auto& assetManager = Project::GetActive()->GetAssetManager();
				const auto& mesh = assetManager->GetOrLoadAsset<Mesh>(component.MeshHandle);

				ImGui::TextDisabled("%s", mesh->GetName().c_str());
				ImGui::SameLine();
				if (ImGui::Button("X"))
					component.MeshHandle = 0;
			}
			else
			{
				ImGui::Button("Mesh Handle", ImVec2(100.0f, 0.0f));
				if (ImGui::BeginDragDropTarget())
				{
					// Assets dragged from the content browser carry their handle.
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_HANDLE"))
					{
						const AssetHandle handle = *static_cast<const uint64_t*>(payload->Data);
						const auto& assetManager = Project::GetActive()->GetAssetManager();
						if (assetManager->GetMetadata(handle).Type == AssetType::Mesh)
							component.MeshHandle = handle;
						else
							Log::Warn("Dropped asset is not a mesh; ignoring.");
					}
					ImGui::EndDragDropTarget();
				}
				
				if (ImGui::Button("Create mesh", ImVec2(100.0f, 0.0f)))
					ImGui::OpenPopup("CreateMesh");

				if (ImGui::BeginPopup("CreateMesh"))
				{
					if (ImGui::MenuItem("Cone"))
					{
						component.MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cone);
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("Cube"))
					{
						component.MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cube);
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("Cylinder"))
					{
						component.MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Cylinder);
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("Sphere"))
					{
						component.MeshHandle = static_cast<uint64_t>(MeshPrimitiveType::Sphere);
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}
			}
		});

		DrawComponent<CameraComponent>(entity, [](auto& component)
		{
			ImGui::Checkbox("Primary", &component.Primary);

			float verticalFov = component.Camera.GetPerspectiveVerticalFov();
			if (ImGui::DragFloat("Vertical FOV", &verticalFov, 0.1f, 1.0f, 179.0f))
				component.Camera.SetPerspectiveVerticalFov(verticalFov);

			float nearClip = component.Camera.GetPerspectiveNearClip();
			if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.001f, 0.0f))
				component.Camera.SetPerspectiveNearClip(nearClip);

			float farClip = component.Camera.GetPerspectiveFarClip();
			if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, 0.0f, 0.0f))
				component.Camera.SetPerspectiveFarClip(farClip);
		});

		DrawComponent<ScriptComponent>(entity, [entity](auto& component)
		{
			if (!ScriptEngine::IsInitialized())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Script runtime not initialized");
				return;
			}

			auto& scriptEngine = ScriptEngine::Get();
			const auto& classes = scriptEngine.GetClasses();
			const bool valid = scriptEngine.IsValidScriptClass(component.ClassName);

			const char* preview = component.ClassName.empty() ? "(none)" : component.ClassName.c_str();
			if (ImGui::BeginCombo("Class", preview))
			{
				for (const auto& cls : classes)
				{
					const bool selected = cls.GetFullName() == component.ClassName;
					if (ImGui::Selectable(cls.GetFullName().c_str(), selected))
						component.ClassName = cls.GetFullName();
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (!component.ClassName.empty() && !valid)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Unknown script class");
				return;
			}

			const auto classIndex = scriptEngine.FindClassIndex(component.ClassName);
			if (classIndex < 0)
				return;

			const auto& fields = classes[classIndex].GetFields();
			if (fields.empty())
				return;

			// The side table owns the editor-time field values (keyed by UUID).
			const auto uuid = entity.GetUUID();
			auto& fieldMap = scriptEngine.GetFieldMap(uuid);

			// While a script is running, its engine-owned instance is the source
			// of truth, so we show and edit that value directly. Editing it does
			// not touch the serialized side table, so stopping play restores the
			// editor-time values. A null handle means edit mode (or a script that
			// failed to instantiate) — fall back to the side table.
			ScriptInstance* instance = scriptEngine.GetEntityInstance(uuid);

			if (ImGui::BeginTable("##ScriptFields", 2))
			{
				for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); i++)
				{
					const auto& field = fields[i];
					if (field.Type == EppoScriptCore::ScriptFieldType::None)
						continue;

					auto& stored = fieldMap[field.Name];
					if (stored.Type != field.Type)
					{
						stored = ScriptFieldValue{};
						stored.Type = field.Type;
					}

					ImGui::TableNextRow();
					ImGui::PushID(field.Name.c_str());

					if (instance)
					{
						// Seed the widget from the running instance's current value
						// (the script may have changed it), then push edits back.
						ScriptFieldValue liveValue = stored;
						instance->GetFieldValue(i, liveValue.Buffer.data());
						if (Utils::DrawScriptField(field, liveValue))
							instance->SetFieldValue(i, liveValue.Buffer.data());
					}
					else
					{
						Utils::DrawScriptField(field, stored);
					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		});
	}

	template<typename T>
	auto PropertyPanel::DrawAddComponentEntry(const std::string& label) const -> void
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
	auto PropertyPanel::DrawComponent(Entity entity, FN uiFn, const std::string& tag) -> void
	{
		if (!entity.HasComponent<T>())
			return;

		auto& c = entity.GetComponent<T>();

		std::string label = tag.empty() ? Utils::GetComponentString<T>() : tag;

		bool closedHeader = true;
		if (ImGui::CollapsingHeader(label.c_str(), &closedHeader, ImGuiTreeNodeFlags_DefaultOpen))
			uiFn(c);

		if (!closedHeader)
			entity.RemoveComponent<T>();
	}
}