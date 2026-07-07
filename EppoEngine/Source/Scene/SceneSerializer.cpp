#include "pch.h"
#include "Scene/SceneSerializer.h"

#include "Scripting/ScriptEngine.h"
#include "Utility/Json.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Eppo
{
	using FT = EppoScriptCore::ScriptFieldType;

	namespace Utils
	{
		// Serializes a single script field value to JSON as its natural type.
		auto SerializeScriptFieldData(const ScriptFieldValue& value) -> json
		{
			switch (value.Type)
			{
				case FT::Float:   return value.Get<float>();
				case FT::Double:  return value.Get<double>();
				case FT::Bool:    return value.Get<uint8_t>() != 0;
				case FT::Char:    return value.Get<uint16_t>();
				case FT::Int16:   return value.Get<int16_t>();
				case FT::Int32:   return value.Get<int32_t>();
				case FT::Int64:   return value.Get<int64_t>();
				case FT::Byte:    return value.Get<uint8_t>();
				case FT::UInt16:  return value.Get<uint16_t>();
				case FT::UInt32:  return value.Get<uint32_t>();
				case FT::UInt64:  return value.Get<uint64_t>();
				case FT::Vector2: return value.Get<glm::vec2>();
				case FT::Vector3: return value.Get<glm::vec3>();
				case FT::Vector4: return value.Get<glm::vec4>();
				case FT::Entity:  return value.Get<uint64_t>();
				default:          return nullptr;
			}
		}

		// Reads a JSON value back into a typed field buffer.
		auto DeserializeScriptFieldData(const json& data, const FT type) -> ScriptFieldValue
		{
			ScriptFieldValue value;
			value.Type = type;

			switch (type)
			{
				case FT::Float:   value.Set<float>(data.get<float>()); break;
				case FT::Double:  value.Set<double>(data.get<double>()); break;
				case FT::Bool:    value.Set<uint8_t>(data.get<bool>() ? 1 : 0); break;
				case FT::Char:    value.Set<uint16_t>(data.get<uint16_t>()); break;
				case FT::Int16:   value.Set<int16_t>(data.get<int16_t>()); break;
				case FT::Int32:   value.Set<int32_t>(data.get<int32_t>()); break;
				case FT::Int64:   value.Set<int64_t>(data.get<int64_t>()); break;
				case FT::Byte:    value.Set<uint8_t>(data.get<uint8_t>()); break;
				case FT::UInt16:  value.Set<uint16_t>(data.get<uint16_t>()); break;
				case FT::UInt32:  value.Set<uint32_t>(data.get<uint32_t>()); break;
				case FT::UInt64:  value.Set<uint64_t>(data.get<uint64_t>()); break;
				case FT::Vector2: value.Set<glm::vec2>(data.get<glm::vec2>()); break;
				case FT::Vector3: value.Set<glm::vec3>(data.get<glm::vec3>()); break;
				case FT::Vector4: value.Set<glm::vec4>(data.get<glm::vec4>()); break;
				case FT::Entity:  value.Set<uint64_t>(data.get<uint64_t>()); break;
				default:
					Log::Warn("Skipping script field with unsupported type {}", static_cast<uint8_t>(type));
					break;
			}

			return value;
		}
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_SceneContext(scene)
	{}

	auto SceneSerializer::Serialize(const std::filesystem::path& path) const -> bool
	{
		EP_PROFILE_FN("SceneSerializer::Serialize");

		std::string sceneName = path.stem().string();
		Log::Info("Serializing scene '{}'", sceneName);

		json data;
		data["Scene"]["Name"] = sceneName;
		data["Scene"]["Handle"] = m_SceneContext->Handle;

		const auto& env = m_SceneContext->GetEnvironment();
		data["Scene"]["Environment"]["SkyboxHandle"] = env.SkyboxHandle;
		data["Scene"]["Environment"]["ZenithColor"] = env.ZenithColor;
		data["Scene"]["Environment"]["HorizonColor"] = env.HorizonColor;
		data["Scene"]["Environment"]["GroundColor"] = env.GroundColor;
		data["Scene"]["Environment"]["AmbientIntensity"] = env.AmbientIntensity;

		auto entities = json::array();

		m_SceneContext->m_Registry.sort<IDComponent>(
			[](const auto& lhs, const auto& rhs) 
			{
				return lhs.ID < rhs.ID;
			}
		);
		
		const auto view = m_SceneContext->m_Registry.view<IDComponent>();
		for (const auto e : view)
		{
			const Entity entity(e, m_SceneContext.get());
			if (!entity)
				continue;

			SerializeEntity(entities, entity);
		}

		data["Scene"]["Entities"] = entities;
		FS::WriteText(path, data.dump(4), true);

		return true;
	}

	auto SceneSerializer::Deserialize(const std::filesystem::path& path) const -> bool
	{
		EP_PROFILE_FN("SceneSerializer::Deserialize");

		std::ifstream stream(path);
		json data;

		try
		{
			data = json::parse(stream);
		}
		catch (const json::exception& ex)
		{
			Log::Error("Failed to parse scene file '{}'!", path);
			Log::Error("Parse error: {}", ex.what());
			return false;
		}

		const auto sceneName = data["Scene"]["Name"].get<std::string>();
		Log::Info("Deserializing scene '{}'", sceneName);

		// Environment is optional: scenes saved before it existed keep the defaults.
		if (data["Scene"].contains("Environment"))
		{
			const auto& envJson = data["Scene"]["Environment"];
			auto& env = m_SceneContext->GetEnvironment();

			if (envJson.contains("SkyboxHandle"))
				env.SkyboxHandle = envJson["SkyboxHandle"].get<AssetHandle>();
			if (envJson.contains("ZenithColor"))
				env.ZenithColor = envJson["ZenithColor"].get<glm::vec3>();
			if (envJson.contains("HorizonColor"))
				env.HorizonColor = envJson["HorizonColor"].get<glm::vec3>();
			if (envJson.contains("GroundColor"))
				env.GroundColor = envJson["GroundColor"].get<glm::vec3>();
			if (envJson.contains("AmbientIntensity"))
				env.AmbientIntensity = envJson["AmbientIntensity"].get<float>();
		}

		auto& entities = data["Scene"]["Entities"];
		if (entities.empty())
		{
			Log::Warn("Scene '{}' has no entities, are you sure this is correct?", sceneName);
			return true;
		}

		for (auto& entity : entities)
		{
			// ID
			if (!entity.contains("IDComponent"))
			{
				Log::Warn("Entity did not have a unique identifier, skipping...");
				continue;
			}

			const auto entityId = entity["IDComponent"]["ID"].get<UUID>();

			// Tag
			if (!entity.contains("TagComponent"))
			{
				Log::Warn("Entity did not have a tag, skipping...");
				continue;
			}

			const auto entityName = entity["TagComponent"]["Tag"].get<std::string>();

			// Create new entity
			Entity newEntity = m_SceneContext->CreateEntityWithUUID(entityId, entityName);
			Log::Info("Deserializing entity '{}' ({})", entityName, entityId);

			if (entity.contains("TransformComponent"))
			{
				auto& c = entity["TransformComponent"];
				auto& nc = newEntity.GetComponent<TransformComponent>();
				nc.Translation = c["Translation"].get<glm::vec3>();
				nc.Rotation = c["Rotation"].get<glm::vec3>();
				nc.Scale = c["Scale"].get<glm::vec3>();
			}

			if (entity.contains("MeshComponent"))
			{
				auto& c = entity["MeshComponent"];
				auto& nc = newEntity.AddComponent<MeshComponent>();
				nc.MeshHandle = c["MeshHandle"].get<AssetHandle>();
			}

			if (entity.contains("CameraComponent"))
			{
				auto& c = entity["CameraComponent"];
				auto& nc = newEntity.AddComponent<CameraComponent>();
				nc.Primary = c["Primary"].get<bool>();
				nc.Camera.SetPerspective(
					c["VerticalFov"].get<float>(),
					c["NearClip"].get<float>(),
					c["FarClip"].get<float>()
				);
			}

			if (entity.contains("PointLightComponent"))
			{
				auto& c = entity["PointLightComponent"];
				auto& nc = newEntity.AddComponent<PointLightComponent>();
				nc.Color = c["Color"].get<glm::vec3>();
				nc.Intensity = c["Intensity"].get<float>();
			}

			if (entity.contains("ScriptComponent"))
			{
				auto& c = entity["ScriptComponent"];
				auto& nc = newEntity.AddComponent<ScriptComponent>();
				nc.ClassName = c["ClassName"].get<std::string>();

				if (c.contains("Fields") && c["Fields"].is_array() && !c["Fields"].empty())
				{
					if (!ScriptEngine::IsInitialized())
					{
						Log::Warn("Script field values for entity '{}' were not loaded: the script runtime is not initialized.", newEntity.GetName());
					}
					else
					{
						auto& fieldMap = ScriptEngine::Get().GetFieldMap(newEntity.GetUUID());
						for (auto& field : c["Fields"])
						{
							const auto name = field["Name"].get<std::string>();
							const auto type = static_cast<FT>(field["Type"].get<uint8_t>());
							fieldMap[name] = Utils::DeserializeScriptFieldData(field["Data"], type);
						}
					}
				}
			}
		}

		return true;
	}

	auto SceneSerializer::SerializeEntity(nlohmann::json& data, Entity entity) const -> void
	{
		EP_PROFILE_FN("SceneSerializer::SerializeEntity");
		EP_ASSERT(entity.HasComponent<IDComponent>() && entity.HasComponent<TagComponent>());

		Log::Info("Serializing entity '{}' ({})", entity.GetName(), entity.GetUUID());
		json e;

		e["IDComponent"]["ID"] = entity.GetUUID();
		e["TagComponent"]["Tag"] = entity.GetName();

		if (entity.HasComponent<TransformComponent>())
		{
			const auto& c = entity.GetComponent<TransformComponent>();
			e["TransformComponent"]["Translation"] = c.Translation;
			e["TransformComponent"]["Rotation"] = c.Rotation;
			e["TransformComponent"]["Scale"] = c.Scale;
		}

		if (entity.HasComponent<MeshComponent>())
		{
			const auto& c = entity.GetComponent<MeshComponent>();
			e["MeshComponent"]["MeshHandle"] = c.MeshHandle;
		}

		if (entity.HasComponent<CameraComponent>())
		{
			const auto& c = entity.GetComponent<CameraComponent>();
			e["CameraComponent"]["Primary"] = c.Primary;
			e["CameraComponent"]["VerticalFov"] = c.Camera.GetPerspectiveVerticalFov();
			e["CameraComponent"]["NearClip"] = c.Camera.GetPerspectiveNearClip();
			e["CameraComponent"]["FarClip"] = c.Camera.GetPerspectiveFarClip();
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			const auto& c = entity.GetComponent<PointLightComponent>();
			e["PointLightComponent"]["Color"] = c.Color;
			e["PointLightComponent"]["Intensity"] = c.Intensity;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			const auto& c = entity.GetComponent<ScriptComponent>();
			e["ScriptComponent"]["ClassName"] = c.ClassName;

			auto fields = json::array();
			const auto* fieldMap = ScriptEngine::IsInitialized() ? ScriptEngine::Get().TryGetFieldMap(entity.GetUUID()) : nullptr;
			if (fieldMap)
			{
				for (const auto& [name, value] : *fieldMap)
				{
					if (value.Type == FT::None)
						continue;

					json field;
					field["Name"] = name;
					field["Type"] = static_cast<uint8_t>(value.Type);
					field["Data"] = Utils::SerializeScriptFieldData(value);
					fields.emplace_back(field);
				}
			}
			e["ScriptComponent"]["Fields"] = fields;
		}

		data.emplace_back(e);
	}
}