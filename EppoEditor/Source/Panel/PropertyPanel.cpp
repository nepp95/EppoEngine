#include "PropertyPanel.h"

#include "Scripting/ScriptClass.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptField.h"
#include "Scripting/ScriptInstance.h"

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
			DrawAddComponentEntry<ScriptComponent>("Script");
			DrawAddComponentEntry<RigidBodyComponent>("Rigid Body");
			DrawAddComponentEntry<CameraComponent>("Camera");
			DrawAddComponentEntry<PointLightComponent>("Point Light");

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

				ImGui::PopID();
				ImGui::EndTable();
			}
		});

		DrawComponent<SpriteComponent>(entity, [](auto& component)
		{
			if (component.TextureHandle)
			{
				ImGui::TextDisabled(Project::GetActive()->GetAssetManagerEditor()->GetMetadata(component.TextureHandle).Filepath.string().c_str());
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

						//Ref<Texture> texture = AssetManager::GetAsset<Texture>(texturePath);
						//component.TextureHandle = texture->Handle;
					}
					ImGui::EndDragDropTarget();
				}
			}

			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
		});

		DrawComponent<MeshComponent>(entity, [](auto& component)
		{
			if (component.MeshHandle)
			{
				ImGui::TextDisabled(Project::GetActive()->GetAssetManagerEditor()->GetMetadata(component.MeshHandle).Filepath.string().c_str());
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
						auto handle = payload->Data;
						component.MeshHandle = *(AssetHandle*)handle;
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
		}, "Directional Light");

		DrawComponent<ScriptComponent>(entity, [entity, scene = GetSceneContext()](auto& component) mutable
		{
			std::string& name = component.ClassName;

			if (ImGui::BeginCombo("Class", name.c_str()))
			{
				for (const auto& [className, scriptClass] : ScriptEngine::GetEntityClasses())
				{
					bool isSelected = className == name;

					if (ImGui::Selectable(className.c_str(), isSelected))
						name = className;

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			UUID uuid = entity.GetUUID();

			if (scene->IsRunning())
			{
				Ref<ScriptInstance> instance = ScriptEngine::GetEntityInstance(uuid);
				if (instance)
				{
					const auto& fields = instance->GetScriptClass()->GetFields();
					for (const auto& [name, field] : fields)
					{
						switch (field.Type)
						{
							case ScriptFieldType::Float:
							{
								float data = instance->GetFieldValue<float>(name);
								if (ImGui::InputFloat(name.c_str(), &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Double:
							{
								double data = instance->GetFieldValue<double>(name);
								if (ImGui::InputDouble(name.c_str(), &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Bool:
							{
								bool data = instance->GetFieldValue<bool>(name);
								if (ImGui::Checkbox(name.c_str(), &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Char:
							{
								int8_t data = instance->GetFieldValue<int8_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S8, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Int16:
							{
								int16_t data = instance->GetFieldValue<int16_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S16, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Int32:
							{
								int32_t data = instance->GetFieldValue<int32_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S32, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Int64:
							{
								int64_t data = instance->GetFieldValue<int64_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S64, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Byte:
							{
								uint8_t data = instance->GetFieldValue<uint8_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U8, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::UInt16:
							{
								uint16_t data = instance->GetFieldValue<uint16_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U16, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::UInt32:
							{
								uint32_t data = instance->GetFieldValue<uint32_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U32, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::UInt64:
							{
								uint64_t data = instance->GetFieldValue<uint64_t>(name);
								if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U64, &data))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Vector2:
							{
								glm::vec2 data = instance->GetFieldValue<glm::vec2>(name);
								if (ImGui::InputFloat2(name.c_str(), glm::value_ptr(data)))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Vector3:
							{
								glm::vec3 data = instance->GetFieldValue<glm::vec3>(name);
								if (ImGui::InputFloat3(name.c_str(), glm::value_ptr(data)))
									instance->SetFieldValue(name, data);
								break;
							}

							case ScriptFieldType::Vector4:
							{
								glm::vec4 data = instance->GetFieldValue<glm::vec4>(name);
								if (ImGui::InputFloat4(name.c_str(), glm::value_ptr(data)))
									instance->SetFieldValue(name, data);
								break;
							}
						}
					}
				}
			} else
			{
				Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(name);
				if (entityClass)
				{
					const auto& fields = entityClass->GetFields();
					auto& entityFields = ScriptEngine::GetScriptFieldMap(uuid);

					for (const auto& [name, field] : fields)
					{
						auto it = entityFields.find(name);
						if (it != entityFields.end())
						{
							ScriptFieldInstance& scriptField = it->second;

							switch (field.Type)
							{
								case ScriptFieldType::Float:
								{
									float data = scriptField.GetValue<float>();
									if (ImGui::InputFloat(name.c_str(), &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Double:
								{
									double data = scriptField.GetValue<double>();
									if (ImGui::InputDouble(name.c_str(), &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Bool:
								{
									bool data = scriptField.GetValue<bool>();
									if (ImGui::Checkbox(name.c_str(), &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Char:
								{
									int8_t data = scriptField.GetValue<int8_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S8, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Int16:
								{
									int16_t data = scriptField.GetValue<int16_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S16, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Int32:
								{
									int32_t data = scriptField.GetValue<int32_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S32, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Int64:
								{
									int64_t data = scriptField.GetValue<int64_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S64, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Byte:
								{
									uint8_t data = scriptField.GetValue<uint8_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U8, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::UInt16:
								{
									uint16_t data = scriptField.GetValue<uint16_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U16, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::UInt32:
								{
									uint32_t data = scriptField.GetValue<uint32_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U32, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::UInt64:
								{
									uint64_t data = scriptField.GetValue<uint64_t>();
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U64, &data))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Vector2:
								{
									glm::vec2 data = scriptField.GetValue<glm::vec2>();
									if (ImGui::InputFloat2(name.c_str(), glm::value_ptr(data)))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Vector3:
								{
									glm::vec3 data = scriptField.GetValue<glm::vec3>();
									if (ImGui::InputFloat3(name.c_str(), glm::value_ptr(data)))
										scriptField.SetValue(data);
									break;
								}

								case ScriptFieldType::Vector4:
								{
									glm::vec4 data = scriptField.GetValue<glm::vec4>();
									if (ImGui::InputFloat4(name.c_str(), glm::value_ptr(data)))
										scriptField.SetValue(data);
									break;
								}
							}
						} else
						{
							switch (field.Type)
							{
								case ScriptFieldType::Float:
								{
									float data = 0.0f;
									if (ImGui::InputFloat(name.c_str(), &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Double:
								{
									double data = 0.0;
									if (ImGui::InputDouble(name.c_str(), &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Bool:
								{
									bool data = false;
									if (ImGui::Checkbox(name.c_str(), &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Char:
								{
									int8_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S8, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Int16:
								{
									int16_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S16, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Int32:
								{
									int32_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S32, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Int64:
								{
									int64_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_S64, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Byte:
								{
									uint8_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U8, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::UInt16:
								{
									uint16_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U16, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::UInt32:
								{
									uint32_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U32, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::UInt64:
								{
									uint64_t data = 0;
									if (ImGui::InputScalar(name.c_str(), ImGuiDataType_U64, &data))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Vector2:
								{
									glm::vec2 data = glm::vec2(0.0f);
									if (ImGui::InputFloat2(name.c_str(), glm::value_ptr(data)))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Vector3:
								{
									glm::vec3 data = glm::vec3(0.0f);
									if (ImGui::InputFloat3(name.c_str(), glm::value_ptr(data)))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}

								case ScriptFieldType::Vector4:
								{
									glm::vec4 data = glm::vec4(0.0f);
									if (ImGui::InputFloat4(name.c_str(), glm::value_ptr(data)))
									{
										ScriptFieldInstance& scriptField = entityFields[name];
										scriptField.Field = field;
										scriptField.SetValue(data);
									}
									break;
								}
							}
						}
					}
				}
			}
		});

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
		}, "Rigid Body");

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

		DrawComponent<PointLightComponent>(entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
		}, "Point Light");
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
