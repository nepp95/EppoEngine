#include "pch.h"
#include "Scene/SceneSerializer.h"

#include "Asset/PackFormat.h"
#include "Scripting/ScriptEngine.h"
#include "Utility/Json.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Eppo
{
	static std::vector<std::string> s_RelationshipRepairNotices;
	static constexpr uint16_t MeshComponentBit = 1 << 0;
	static constexpr uint16_t CameraComponentBit = 1 << 1;
	static constexpr uint16_t PointLightComponentBit = 1 << 2;
	static constexpr uint16_t ScriptComponentBit = 1 << 3;
	static constexpr uint16_t RelationshipComponentBit = 1 << 4;
	static constexpr uint16_t RigidBodyComponentBit = 1 << 5;
	static constexpr uint16_t BoxColliderComponentBit = 1 << 6;
	static constexpr uint16_t SphereColliderComponentBit = 1 << 7;
	static constexpr uint16_t CapsuleColliderComponentBit = 1 << 8;
	static constexpr uint16_t CylinderColliderComponentBit = 1 << 9;
	static constexpr uint16_t KnownComponentBits = (1 << 10) - 1;

	template<typename T>
	auto WriteRaw(BufferWriter& writer, const T& value) -> bool
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return writer.Write(value);
	}

	template<typename T>
	auto ReadRaw(BufferReader& reader, T& value) -> bool
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return reader.Read(value);
	}

	namespace Utils
	{
		// Serializes a single script field value to JSON as its natural type.
		auto SerializeScriptFieldData(const ScriptFieldValue& value) -> json
		{
			switch (value.Type)
			{
				case ScriptFieldType::Float:   return value.Get<float>();
				case ScriptFieldType::Double:  return value.Get<double>();
				case ScriptFieldType::Bool:    return value.Get<uint8_t>() != 0;
				case ScriptFieldType::Char:    return value.Get<uint16_t>();
				case ScriptFieldType::Int16:   return value.Get<int16_t>();
				case ScriptFieldType::Int32:   return value.Get<int32_t>();
				case ScriptFieldType::Int64:   return value.Get<int64_t>();
				case ScriptFieldType::Byte:    return value.Get<uint8_t>();
				case ScriptFieldType::UInt16:  return value.Get<uint16_t>();
				case ScriptFieldType::UInt32:  return value.Get<uint32_t>();
				case ScriptFieldType::UInt64:  return value.Get<uint64_t>();
				case ScriptFieldType::Vector2: return value.Get<glm::vec2>();
				case ScriptFieldType::Vector3: return value.Get<glm::vec3>();
				case ScriptFieldType::Vector4: return value.Get<glm::vec4>();
				case ScriptFieldType::Entity:  return value.Get<uint64_t>();
				default:          return nullptr;
			}
		}

		// Reads a JSON value back into a typed field buffer.
		auto DeserializeScriptFieldData(const json& data, const ScriptFieldType type) -> ScriptFieldValue
		{
			ScriptFieldValue value;
			value.Type = type;

			switch (type)
			{
				case ScriptFieldType::Float:   value.Set<float>(data.get<float>()); break;
				case ScriptFieldType::Double:  value.Set<double>(data.get<double>()); break;
				case ScriptFieldType::Bool:    value.Set<uint8_t>(data.get<bool>() ? 1 : 0); break;
				case ScriptFieldType::Char:    value.Set<uint16_t>(data.get<uint16_t>()); break;
				case ScriptFieldType::Int16:   value.Set<int16_t>(data.get<int16_t>()); break;
				case ScriptFieldType::Int32:   value.Set<int32_t>(data.get<int32_t>()); break;
				case ScriptFieldType::Int64:   value.Set<int64_t>(data.get<int64_t>()); break;
				case ScriptFieldType::Byte:    value.Set<uint8_t>(data.get<uint8_t>()); break;
				case ScriptFieldType::UInt16:  value.Set<uint16_t>(data.get<uint16_t>()); break;
				case ScriptFieldType::UInt32:  value.Set<uint32_t>(data.get<uint32_t>()); break;
				case ScriptFieldType::UInt64:  value.Set<uint64_t>(data.get<uint64_t>()); break;
				case ScriptFieldType::Vector2: value.Set<glm::vec2>(data.get<glm::vec2>()); break;
				case ScriptFieldType::Vector3: value.Set<glm::vec3>(data.get<glm::vec3>()); break;
				case ScriptFieldType::Vector4: value.Set<glm::vec4>(data.get<glm::vec4>()); break;
				case ScriptFieldType::Entity:  value.Set<uint64_t>(data.get<uint64_t>()); break;
				default:
					Log::Warn("Skipping script field with unsupported type {}", static_cast<uint8_t>(type));
					break;
			}

			return value;
		}
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene, SceneSerializerOptions options)
		: m_SceneContext(scene), m_Options(options)
	{}

	auto SceneSerializer::FindScriptFields(const UUID entityId) const -> const ScriptFieldMap*
	{
		if (m_Options.ScriptFields)
		{
			const auto fields = m_Options.ScriptFields->find(entityId);
			return fields == m_Options.ScriptFields->end() ? nullptr : &fields->second;
		}

		return ScriptEngine::IsInitialized() ? ScriptEngine::Get().TryGetFieldMap(entityId) : nullptr;
	}

	auto SceneSerializer::GetScriptFields(const UUID entityId) const -> ScriptFieldMap*
	{
		if (m_Options.ScriptFields)
			return &(*m_Options.ScriptFields)[entityId];
		return ScriptEngine::IsInitialized() ? &ScriptEngine::Get().GetFieldMap(entityId) : nullptr;
	}

	auto SceneSerializer::ConsumeRelationshipRepairNotices() -> std::vector<std::string>
	{
		std::vector<std::string> notices = std::move(s_RelationshipRepairNotices);
		s_RelationshipRepairNotices.clear();
		return notices;
	}

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

		m_SceneContext->SortEntitiesByID();

		m_SceneContext->ForEachEntity([&](Entity entity)
		{
			SerializeEntity(entities, entity);
		});

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

			if (entity.contains("RelationshipComponent"))
			{
				auto& c = entity["RelationshipComponent"];
				auto& nc = newEntity.AddComponent<RelationshipComponent>();
				nc.Parent = c["Parent"].get<UUID>();
				if (c.contains("Children"))
				{
					for (auto& child : c["Children"])
						nc.Children.emplace_back(child.get<UUID>());
				}
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
                    if (ScriptFieldMap* fieldMap = GetScriptFields(newEntity.GetUUID()); !fieldMap)
					{
						Log::Warn("Script field values for entity '{}' were not loaded: the script runtime is not initialized.", newEntity.GetName());
					}
					else
					{
						for (auto& field : c["Fields"])
						{
							const auto name = field["Name"].get<std::string>();
							const auto type = static_cast<ScriptFieldType>(field["Type"].get<uint8_t>());
							(*fieldMap)[name] = Utils::DeserializeScriptFieldData(field["Data"], type);
						}
					}
				}
			}

			if (entity.contains("RigidBodyComponent"))
			{
				auto& c = entity["RigidBodyComponent"];
				auto& nc = newEntity.AddComponent<RigidBodyComponent>();
				nc.Type = static_cast<RigidBodyComponent::BodyType>(c["Type"].get<uint8_t>());
				nc.GravityScale = c["GravityScale"].get<float>();
				nc.LinearDamping = c["LinearDamping"].get<float>();
				nc.AngularDamping = c["AngularDamping"].get<float>();
			}

			if (entity.contains("BoxColliderComponent"))
			{
				auto& c = entity["BoxColliderComponent"];
				auto& nc = newEntity.AddComponent<BoxColliderComponent>();
                nc.HalfSize = c["HalfSize"].get<glm::vec3>();
				nc.Offset = c["Offset"].get<glm::vec3>();
				nc.Density = c["Density"].get<float>();
				nc.Friction = c["Friction"].get<float>();
				nc.Restitution = c["Restitution"].get<float>();
			}

			if (entity.contains("SphereColliderComponent"))
			{
				auto& c = entity["SphereColliderComponent"];
				auto& nc = newEntity.AddComponent<SphereColliderComponent>();
				nc.Radius = c["Radius"].get<float>();
				nc.Offset = c["Offset"].get<glm::vec3>();
				nc.Density = c["Density"].get<float>();
				nc.Friction = c["Friction"].get<float>();
				nc.Restitution = c["Restitution"].get<float>();
			}

			if (entity.contains("CapsuleColliderComponent"))
			{
				auto& c = entity["CapsuleColliderComponent"];
				auto& nc = newEntity.AddComponent<CapsuleColliderComponent>();
				nc.Radius = c["Radius"].get<float>();
				nc.Height = c["Height"].get<float>();
				nc.Offset = c["Offset"].get<glm::vec3>();
				nc.Density = c["Density"].get<float>();
				nc.Friction = c["Friction"].get<float>();
				nc.Restitution = c["Restitution"].get<float>();
			}

			if (entity.contains("CylinderColliderComponent"))
			{
				auto& c = entity["CylinderColliderComponent"];
				auto& nc = newEntity.AddComponent<CylinderColliderComponent>();
				nc.Radius = c["Radius"].get<float>();
				nc.Height = c["Height"].get<float>();
				nc.Offset = c["Offset"].get<glm::vec3>();
				nc.Density = c["Density"].get<float>();
				nc.Friction = c["Friction"].get<float>();
				nc.Restitution = c["Restitution"].get<float>();
			}

		}

		RepairRelationships(sceneName);

		return true;
	}

	auto SceneSerializer::Serialize(BufferWriter& writer) const -> bool
	{
		static_assert(std::is_trivially_copyable_v<EnvironmentSettings>);
		static_assert(sizeof(EnvironmentSettings) == sizeof(AssetHandle) + sizeof(glm::vec3) * 3 + sizeof(float));
		static_assert(sizeof(TransformComponent) == sizeof(glm::vec3) * 3);
		static_assert(sizeof(MeshComponent) == sizeof(AssetHandle));
		static_assert(sizeof(PointLightComponent) == sizeof(glm::vec3) + sizeof(float));
		static_assert(offsetof(RigidBodyComponent, GravityScale) == sizeof(RigidBodyComponent::BodyType));
		static_assert(sizeof(RigidBodyComponent) == sizeof(RigidBodyComponent::BodyType) + sizeof(float) * 3);
		static_assert(sizeof(BoxColliderComponent) == sizeof(glm::vec3) * 2 + sizeof(float) * 3);
		static_assert(sizeof(SphereColliderComponent) == sizeof(float) + sizeof(glm::vec3) + sizeof(float) * 3);
		static_assert(sizeof(CapsuleColliderComponent) == sizeof(float) * 2 + sizeof(glm::vec3) + sizeof(float) * 3);
		static_assert(sizeof(CylinderColliderComponent) == sizeof(float) * 2 + sizeof(glm::vec3) + sizeof(float) * 3);

		std::vector<Entity> entities;
		m_SceneContext->SortEntitiesByID();
		m_SceneContext->ForEachEntity([&](const Entity entity)
		{
			entities.push_back(entity);
		});

		if (entities.size() > std::numeric_limits<uint32_t>::max())
			return false;

		if (!writer.Write(PackFormat::Scene.Magic)
			|| !writer.Write(PackFormat::Scene.Version)
			|| !writer.Write(static_cast<uint64_t>(m_SceneContext->Handle))
			|| !WriteRaw(writer, m_SceneContext->GetEnvironment())
			|| !writer.Write(static_cast<uint32_t>(entities.size())))
			return false;

		for (const Entity entity : entities)
		{
			uint16_t componentMask = 0;
			componentMask |= entity.HasComponent<MeshComponent>() ? MeshComponentBit : 0;
			componentMask |= entity.HasComponent<CameraComponent>() ? CameraComponentBit : 0;
			componentMask |= entity.HasComponent<PointLightComponent>() ? PointLightComponentBit : 0;
			componentMask |= entity.HasComponent<ScriptComponent>() ? ScriptComponentBit : 0;
			componentMask |= entity.HasComponent<RelationshipComponent>() ? RelationshipComponentBit : 0;
			componentMask |= entity.HasComponent<RigidBodyComponent>() ? RigidBodyComponentBit : 0;
			componentMask |= entity.HasComponent<BoxColliderComponent>() ? BoxColliderComponentBit : 0;
			componentMask |= entity.HasComponent<SphereColliderComponent>() ? SphereColliderComponentBit : 0;
			componentMask |= entity.HasComponent<CapsuleColliderComponent>() ? CapsuleColliderComponentBit : 0;
			componentMask |= entity.HasComponent<CylinderColliderComponent>() ? CylinderColliderComponentBit : 0;

			if (!writer.Write(static_cast<uint64_t>(entity.GetUUID()))
				|| !writer.WriteString(entity.GetName())
				|| !WriteRaw(writer, entity.GetComponent<TransformComponent>())
				|| !writer.Write(componentMask))
				return false;

			if (componentMask & MeshComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<MeshComponent>()))
					return false;
			}

			if (componentMask & CameraComponentBit)
			{
				const auto& component = entity.GetComponent<CameraComponent>();
				if (!writer.Write(static_cast<uint8_t>(component.Primary))
					|| !writer.Write(component.Camera.GetPerspectiveVerticalFov())
					|| !writer.Write(component.Camera.GetPerspectiveNearClip())
					|| !writer.Write(component.Camera.GetPerspectiveFarClip()))
					return false;
			}

			if (componentMask & PointLightComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<PointLightComponent>()))
					return false;
			}

			if (componentMask & ScriptComponentBit)
			{
				const auto& component = entity.GetComponent<ScriptComponent>();
				if (!writer.WriteString(component.ClassName))
					return false;

				std::vector<std::pair<std::string, ScriptFieldValue>> fields;
				const auto* fieldMap = FindScriptFields(entity.GetUUID());
				if (fieldMap)
				{
					fields.reserve(fieldMap->size());
					for (const auto& field : *fieldMap)
						fields.push_back(field);
					std::sort(fields.begin(), fields.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
				}

				if (fields.size() > std::numeric_limits<uint32_t>::max() || !writer.Write(static_cast<uint32_t>(fields.size())))
					return false;

				for (const auto& [name, value] : fields)
				{
					const uint32_t size = ScriptFieldTypeSize(value.Type);
					if (value.Type == ScriptFieldType::None || size == 0 || size > value.Buffer.size()
						|| !writer.WriteString(name)
						|| !writer.Write(static_cast<uint8_t>(value.Type))
						|| !writer.WriteBytes(value.Buffer.data(), size))
						return false;
				}
			}

			if (componentMask & RelationshipComponentBit)
			{
				const auto& component = entity.GetComponent<RelationshipComponent>();
				if (component.Children.size() > std::numeric_limits<uint32_t>::max()
					|| !writer.Write(static_cast<uint64_t>(component.Parent))
					|| !writer.Write(static_cast<uint32_t>(component.Children.size())))
					return false;

				for (const UUID child : component.Children)
				{
					if (!writer.Write(static_cast<uint64_t>(child)))
						return false;
				}
			}

			if (componentMask & RigidBodyComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<RigidBodyComponent>()))
					return false;
			}

			if (componentMask & BoxColliderComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<BoxColliderComponent>()))
					return false;
			}

			if (componentMask & SphereColliderComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<SphereColliderComponent>()))
					return false;
			}

			if (componentMask & CapsuleColliderComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<CapsuleColliderComponent>()))
					return false;
			}

			if (componentMask & CylinderColliderComponentBit)
			{
				if (!WriteRaw(writer, entity.GetComponent<CylinderColliderComponent>()))
					return false;
			}
		}

		return writer.IsValid();
	}

	auto SceneSerializer::Deserialize(BufferReader& reader) const -> bool
	{
		uint32_t magic = 0;
		uint32_t version = 0;
		uint64_t sceneHandle = 0;
		EnvironmentSettings environment{};
		uint32_t entityCount = 0;

		if (!reader.Read(magic) || magic != PackFormat::Scene.Magic
			|| !reader.Read(version) || version != PackFormat::Scene.Version
			|| !reader.Read(sceneHandle) || sceneHandle != static_cast<uint64_t>(m_SceneContext->Handle)
			|| !ReadRaw(reader, environment) || !reader.Read(entityCount))
			return false;

		constexpr uint64_t MinimumEntitySize = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(TransformComponent) + sizeof(uint16_t);
		if (entityCount > reader.GetRemaining() / MinimumEntitySize)
			return false;

		m_SceneContext->GetEnvironment() = environment;
		std::unordered_set<UUID> entityIds;

		for (uint32_t i = 0; i < entityCount; i++)
		{
			uint64_t entityId = 0;
			if (!reader.Read(entityId) || entityId == 0 || !entityIds.insert(UUID(entityId)).second)
				return false;

			const std::string tag = reader.ReadString();
			TransformComponent transform{};
			uint16_t componentMask = 0;
			if (!reader.IsValid() || !ReadRaw(reader, transform) || !reader.Read(componentMask) || (componentMask & ~KnownComponentBits) != 0)
				return false;

			Entity entity = m_SceneContext->CreateEntityWithUUID(UUID(entityId), tag);
			entity.GetComponent<TransformComponent>() = transform;

			if (componentMask & MeshComponentBit)
			{
				MeshComponent component{};
				if (!ReadRaw(reader, component))
					return false;
				entity.AddComponent<MeshComponent>(component);
			}

			if (componentMask & CameraComponentBit)
			{
				uint8_t primary = 0;
				float verticalFov = 0.0f;
				float nearClip = 0.0f;
				float farClip = 0.0f;
				if (!reader.Read(primary) || primary > 1 || !reader.Read(verticalFov) || !reader.Read(nearClip) || !reader.Read(farClip))
					return false;
				auto& component = entity.AddComponent<CameraComponent>();
				component.Primary = primary != 0;
				component.Camera.SetPerspective(verticalFov, nearClip, farClip);
			}

			if (componentMask & PointLightComponentBit)
			{
				PointLightComponent component{};
				if (!ReadRaw(reader, component))
					return false;
				entity.AddComponent<PointLightComponent>(component);
			}

			if (componentMask & ScriptComponentBit)
			{
				const std::string className = reader.ReadString();
				uint32_t fieldCount = 0;
				if (!reader.IsValid() || !reader.Read(fieldCount) || fieldCount > reader.GetRemaining() / 6)
					return false;

				ScriptFieldMap fields;
				for (uint32_t fieldIndex = 0; fieldIndex < fieldCount; fieldIndex++)
				{
					const std::string name = reader.ReadString();
					uint8_t serializedType = 0;
					if (!reader.IsValid() || !reader.Read(serializedType))
						return false;

					const auto type = static_cast<ScriptFieldType>(serializedType);
					const uint32_t size = ScriptFieldTypeSize(type);
					if (type == ScriptFieldType::None || type > ScriptFieldType::Entity || size == 0 || size > ScriptFieldValue{}.Buffer.size()
						|| fields.contains(name))
						return false;

					ScriptFieldValue value;
					value.Type = type;
					if (!reader.ReadBytes(value.Buffer.data(), size))
						return false;
					fields.emplace(name, value);
				}

				entity.AddComponent<ScriptComponent>(className);
				if (!fields.empty())
				{
					ScriptFieldMap* fieldMap = GetScriptFields(entity.GetUUID());
					if (!fieldMap)
						return false;
					*fieldMap = std::move(fields);
				}
			}

			if (componentMask & RelationshipComponentBit)
			{
				uint64_t parent = 0;
				uint32_t childCount = 0;
				if (!reader.Read(parent) || !reader.Read(childCount) || childCount > reader.GetRemaining() / sizeof(uint64_t))
					return false;

				auto& component = entity.AddComponent<RelationshipComponent>();
				component.Parent = UUID(parent);
				component.Children.reserve(childCount);
				for (uint32_t childIndex = 0; childIndex < childCount; childIndex++)
				{
					uint64_t child = 0;
					if (!reader.Read(child))
						return false;
					component.Children.emplace_back(child);
				}
			}

			if (componentMask & RigidBodyComponentBit)
			{
				RigidBodyComponent component{};
				if (!ReadRaw(reader, component) || component.Type > RigidBodyComponent::BodyType::Dynamic)
					return false;
				entity.AddComponent<RigidBodyComponent>(component);
			}

			if (componentMask & BoxColliderComponentBit)
			{
				BoxColliderComponent component{};
				if (!ReadRaw(reader, component))
					return false;
				entity.AddComponent<BoxColliderComponent>(component);
			}

			if (componentMask & SphereColliderComponentBit)
			{
				SphereColliderComponent component{};
				if (!ReadRaw(reader, component))
					return false;
				entity.AddComponent<SphereColliderComponent>(component);
			}

			if (componentMask & CapsuleColliderComponentBit)
			{
				CapsuleColliderComponent component{};
				if (!ReadRaw(reader, component))
					return false;
				entity.AddComponent<CapsuleColliderComponent>(component);
			}

			if (componentMask & CylinderColliderComponentBit)
			{
				CylinderColliderComponent component{};
				if (!ReadRaw(reader, component))
					return false;
				entity.AddComponent<CylinderColliderComponent>(component);
			}
		}

		if (!reader.IsValid() || reader.GetRemaining() != 0)
			return false;

		RepairRelationships(std::to_string(sceneHandle));
		return true;
	}

	auto SceneSerializer::RepairRelationships(const std::string& sceneName) const -> void
	{
		std::vector<std::string> repairs;
		bool changed = true;
		while (changed)
		{
			changed = false;
			std::unordered_set<UUID> invalidRelationships;

			m_SceneContext->ForEachEntity([&](Entity entity)
			{
				if (!entity.HasComponent<RelationshipComponent>())
					return;

				const auto& relationship = entity.GetComponent<RelationshipComponent>();
				if (!relationship.Parent)
					return;

				const Entity parent = m_SceneContext->GetEntityByUUID(relationship.Parent);
				const bool listedByParent = parent && parent.HasComponent<RelationshipComponent>() && [&]
				{
					const auto& children = parent.GetComponent<RelationshipComponent>().Children;
					return std::find(children.begin(), children.end(), entity.GetUUID()) != children.end();
				}();

				std::unordered_set<UUID> visited{ entity.GetUUID() };
				Entity ancestor = parent;
				bool cyclic = false;
				while (ancestor)
				{
					if (!visited.insert(ancestor.GetUUID()).second)
					{
						cyclic = true;
						break;
					}

					const UUID ancestorParent = ancestor.HasComponent<RelationshipComponent>()
						? ancestor.GetComponent<RelationshipComponent>().Parent : UUID(0);
					ancestor = ancestorParent ? m_SceneContext->GetEntityByUUID(ancestorParent) : Entity{};
				}

				if (!listedByParent || cyclic)
					invalidRelationships.insert(entity.GetUUID());
			});

			for (const UUID entityId : invalidRelationships)
			{
				Entity entity = m_SceneContext->GetEntityByUUID(entityId);
				if (!entity || !entity.HasComponent<RelationshipComponent>())
					continue;

				repairs.push_back(fmt::format("'{}' had an inconsistent relationship; detached to root.", entity.GetName()));
				m_SceneContext->SetParent(entity, {});
				changed = true;
			}

			m_SceneContext->ForEachEntity([&](Entity entity)
			{
				if (!entity.HasComponent<RelationshipComponent>())
					return;

				auto& relationship = entity.GetComponent<RelationshipComponent>();
				const size_t oldSize = relationship.Children.size();
				std::erase_if(relationship.Children, [&](const UUID childId)
				{
					const Entity child = m_SceneContext->GetEntityByUUID(childId);
					return !child || !child.HasComponent<RelationshipComponent>()
						|| child.GetComponent<RelationshipComponent>().Parent != entity.GetUUID();
				});
				std::unordered_set<UUID> uniqueChildren;
				std::erase_if(relationship.Children, [&](const UUID childId)
				{
					return !uniqueChildren.insert(childId).second;
				});
				changed |= oldSize != relationship.Children.size();

				if (!relationship.Parent && relationship.Children.empty())
				{
					entity.RemoveComponent<RelationshipComponent>();
					changed = true;
				}
			});
		}

		for (const std::string& repair : repairs)
		{
			Log::Warn("Scene '{}' relationship repaired: {}", sceneName, repair);
			if (m_Options.CollectRelationshipRepairNotices)
				s_RelationshipRepairNotices.push_back(fmt::format("Scene '{}': {}", sceneName, repair));
		}
	}

	auto SceneSerializer::SerializeEntity(nlohmann::json& data, const Entity entity) const -> void
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

		if (entity.HasComponent<RelationshipComponent>())
		{
			const auto& c = entity.GetComponent<RelationshipComponent>();
			// Emit only for entities in a hierarchy, keeping flat scenes unchanged.
			if (c.Parent || !c.Children.empty())
			{
				e["RelationshipComponent"]["Parent"] = c.Parent;
				auto children = json::array();
				for (const UUID child : c.Children)
					children.emplace_back(child);
				e["RelationshipComponent"]["Children"] = children;
			}
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			const auto& c = entity.GetComponent<ScriptComponent>();
			e["ScriptComponent"]["ClassName"] = c.ClassName;

			auto fields = json::array();
			const auto* fieldMap = FindScriptFields(entity.GetUUID());
			if (fieldMap)
			{
				for (const auto& [name, value] : *fieldMap)
				{
					if (value.Type == ScriptFieldType::None)
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

		if (entity.HasComponent<RigidBodyComponent>())
		{
			const auto& c = entity.GetComponent<RigidBodyComponent>();
			e["RigidBodyComponent"]["Type"] = static_cast<uint8_t>(c.Type);
			e["RigidBodyComponent"]["GravityScale"] = c.GravityScale;
			e["RigidBodyComponent"]["LinearDamping"] = c.LinearDamping;
			e["RigidBodyComponent"]["AngularDamping"] = c.AngularDamping;
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			const auto& c = entity.GetComponent<BoxColliderComponent>();
            e["BoxColliderComponent"]["HalfSize"] = c.HalfSize;
			e["BoxColliderComponent"]["Offset"] = c.Offset;
			e["BoxColliderComponent"]["Density"] = c.Density;
			e["BoxColliderComponent"]["Friction"] = c.Friction;
			e["BoxColliderComponent"]["Restitution"] = c.Restitution;
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			const auto& c = entity.GetComponent<SphereColliderComponent>();
			e["SphereColliderComponent"]["Radius"] = c.Radius;
			e["SphereColliderComponent"]["Offset"] = c.Offset;
			e["SphereColliderComponent"]["Density"] = c.Density;
			e["SphereColliderComponent"]["Friction"] = c.Friction;
			e["SphereColliderComponent"]["Restitution"] = c.Restitution;
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			const auto& c = entity.GetComponent<CapsuleColliderComponent>();
			e["CapsuleColliderComponent"]["Radius"] = c.Radius;
			e["CapsuleColliderComponent"]["Height"] = c.Height;
			e["CapsuleColliderComponent"]["Offset"] = c.Offset;
			e["CapsuleColliderComponent"]["Density"] = c.Density;
			e["CapsuleColliderComponent"]["Friction"] = c.Friction;
			e["CapsuleColliderComponent"]["Restitution"] = c.Restitution;
		}

		if (entity.HasComponent<CylinderColliderComponent>())
		{
			const auto& c = entity.GetComponent<CylinderColliderComponent>();
			e["CylinderColliderComponent"]["Radius"] = c.Radius;
			e["CylinderColliderComponent"]["Height"] = c.Height;
			e["CylinderColliderComponent"]["Offset"] = c.Offset;
			e["CylinderColliderComponent"]["Density"] = c.Density;
			e["CylinderColliderComponent"]["Friction"] = c.Friction;
			e["CylinderColliderComponent"]["Restitution"] = c.Restitution;
		}

		data.emplace_back(e);
	}
}
