#include "pch.h"
#include "SceneSerializer.h"

#include "Core/Filesystem.h"
#include "Scripting/ScriptClass.h"
#include "Scripting/ScriptEngine.h"

#include <yaml-cpp/yaml.h>

namespace YAML
{
	Emitter& operator<<(Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	Emitter& operator<<(Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	Emitter& operator<<(Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	template<>
	struct convert<glm::vec2>
	{
		static bool decode(const Node& node, glm::vec2& v)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static bool decode(const Node& node, glm::vec3& v)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();

			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static bool decode(const Node& node, glm::vec4& v)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();
			v.w = node[3].as<float>();

			return true;
		}
	};

	template<>
	struct convert<Eppo::UUID>
	{
		static bool decode(const Node& node, Eppo::UUID& uuid)
		{
			if (node.IsSequence())
				return false;

			uuid = node[0].as<uint64_t>();

			return true;
		}
	};
}

namespace Eppo
{
	#define WRITE_SCRIPT_FIELD(FieldType, Type)				\
		case ScriptFieldType::FieldType:					\
			out << scriptField.GetValue<Type>();			\
			break

	#define READ_SCRIPT_FIELD(FieldType, Type)				\
		case ScriptFieldType::FieldType:					\
		{													\
			Type data = scriptField["Data"].as<Type>();		\
			fieldInstance.SetValue(data);					\
			break;											\
		}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_SceneContext(scene)
	{}

	bool SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("SceneSerializer:Serialize");

		std::string sceneName = filepath.stem().string();

		EPPO_INFO("Serializing scene '{}' ({})", sceneName, m_SceneContext->Handle);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << sceneName;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		
		m_SceneContext->m_Registry.sort<IDComponent>([](const auto& lhs, const auto& rhs) { return lhs.ID < rhs.ID; });
		const auto view = m_SceneContext->m_Registry.view<IDComponent>();
		for (const auto e : view)
		{
			const Entity entity(e, m_SceneContext.get());
			if (!entity)
				continue;

			SerializeEntity(out, entity);
		}

		out << YAML::EndSeq << YAML::EndMap;

		Filesystem::WriteText(filepath, out.c_str());

		return true;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath) const
	{
		EPPO_PROFILE_FUNCTION("SceneSerializer:Deserialize");

		YAML::Node data;

		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (YAML::ParserException& e)
		{
			EPPO_ERROR("Failed to load scene file '{}'!", filepath);
			EPPO_ERROR("YAML Error: {}", e.what());
			return false;
		}

		if (!data["Scene"])
		{
			EPPO_ERROR("Failed to load scene file '{}'! Not a scene file!", filepath);
			return false;
		}

		auto sceneName = data["Scene"].as<std::string>();
		EPPO_INFO("Deserializing scene '{}'", sceneName);

		auto entities = data["Entities"];

		if (!entities)
		{
			EPPO_WARN("Scene '{}' has no entities, are you sure this is correct?", sceneName);
			return true;
		}

		for (auto entity : entities)
		{
			UUID uuid = entity["Entity"].as<uint64_t>();
			std::string tag;

			if (auto tagComponent = entity["TagComponent"]; tagComponent)
				tag = tagComponent["Tag"].as<std::string>();

			Entity newEntity = m_SceneContext->CreateEntityWithUUID(uuid, tag);
			EPPO_INFO("Deserializing entity '{}' ({})", tag, uuid);

			if (auto c = entity["TransformComponent"])
			{
				auto& nc = newEntity.GetComponent<TransformComponent>();
				nc.Translation = c["Translation"].as<glm::vec3>();
				nc.Rotation = c["Rotation"].as<glm::vec3>();
				nc.Scale = c["Scale"].as<glm::vec3>();
			}

			if (auto c = entity["SpriteComponent"])
			{
				auto& nc = newEntity.AddComponent<SpriteComponent>();
				nc.Color = c["Color"].as<glm::vec4>();
				nc.TextureHandle = c["TextureHandle"].as<uint64_t>();
			}

			if (auto c = entity["MeshComponent"])
			{
				auto& [meshHandle] = newEntity.AddComponent<MeshComponent>();
				meshHandle = c["MeshHandle"].as<uint64_t>();
			}

			if (auto c = entity["DirectionalLightComponent"])
			{
				auto& [direction, albedoColor, ambientColor, specularColor] = newEntity.AddComponent<DirectionalLightComponent>();
				direction = c["Direction"].as<glm::vec3>();
				albedoColor = c["Albedo"].as<glm::vec4>();
				ambientColor = c["Ambient"].as<glm::vec4>();
				specularColor = c["Specular"].as<glm::vec4>();
			}

			if (auto c = entity["ScriptComponent"])
			{
				auto& [className] = newEntity.AddComponent<ScriptComponent>();
				className = c["ClassName"].as<std::string>();

				if (auto scriptFields = c["Fields"])
				{
					Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(className);
					EPPO_ASSERT(entityClass)

					const auto& fields = entityClass->GetFields();
					auto& entityFields = ScriptEngine::GetScriptFieldMap(uuid);

					for (auto scriptField : scriptFields)
					{
						auto name = scriptField["Name"].as<std::string>();
						auto typeString = scriptField["Type"].as<std::string>();
						ScriptFieldType type = Utils::ScriptFieldTypeFromString(typeString);

						ScriptFieldInstance& fieldInstance = entityFields[name];

						if (fields.find(name) == fields.end())
						{
							EPPO_ERROR("Mono field not found!");
							continue;
						}

						fieldInstance.Field = fields.at(name);

						switch (type)
						{
							READ_SCRIPT_FIELD(Float, float)
							READ_SCRIPT_FIELD(Double, double)
							READ_SCRIPT_FIELD(Bool, bool)
							READ_SCRIPT_FIELD(Char, int8_t)
							READ_SCRIPT_FIELD(Int16, int16_t)
							READ_SCRIPT_FIELD(Int32, int32_t)
							READ_SCRIPT_FIELD(Int64, int64_t)
							READ_SCRIPT_FIELD(Byte, uint8_t)
							READ_SCRIPT_FIELD(UInt16, uint16_t)
							READ_SCRIPT_FIELD(UInt32, uint32_t)
							READ_SCRIPT_FIELD(UInt64, uint64_t)
							READ_SCRIPT_FIELD(Vector2, glm::vec2)
							READ_SCRIPT_FIELD(Vector3, glm::vec3)
							READ_SCRIPT_FIELD(Vector4, glm::vec4)
							READ_SCRIPT_FIELD(Entity, UUID)
						}
					}
				}
			}
			
			if (auto c = entity["RigidBodyComponent"])
			{
				auto& rbc = newEntity.AddComponent<RigidBodyComponent>();
				rbc.Type = static_cast<RigidBodyComponent::BodyType>(c["BodyType"].as<int>());
				rbc.Mass = c["Mass"].as<float>();
			}

			if (auto c = entity["CameraComponent"])
			{
				auto& [camera] = newEntity.AddComponent<CameraComponent>();
				camera.SetProjectionType(static_cast<ProjectionType>(c["ProjectionType"].as<int>()));
				camera.SetPerspectiveFov(c["PerspectiveFov"].as<float>());
				camera.SetPerspectiveNearClip(c["PerspectiveNearClip"].as<float>());
				camera.SetPerspectiveFarClip(c["PerspectiveFarClip"].as<float>());
				camera.SetOrthographicSize(c["OrthographicSize"].as<float>());
				camera.SetOrthographicNearClip(c["OrthographicNearClip"].as<float>());
				camera.SetOrthographicFarClip(c["OrthographicFarClip"].as<float>());
			}

			if (auto c = entity["PointLightComponent"])
			{
				auto& [color] = newEntity.AddComponent<PointLightComponent>();
				color = c["Color"].as<glm::vec4>();
			}
		}

		return true;
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		EPPO_ASSERT(entity.HasComponent<IDComponent>() && entity.HasComponent<TagComponent>());

		EPPO_INFO("Serializing entity '{}' ({})", entity.GetName(), entity.GetUUID());

		out << YAML::BeginMap;

		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent" << YAML::Value;
			out << YAML::BeginMap;

			out << YAML::Key << "Tag" << YAML::Value << entity.GetName();

			out << YAML::EndMap;
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& c = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << c.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << c.Scale;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			out << YAML::Key << "SpriteComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& c = entity.GetComponent<SpriteComponent>();
			out << YAML::Key << "Color" << YAML::Value << c.Color;
			out << YAML::Key << "TextureHandle" << YAML::Value << c.TextureHandle;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& c = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "MeshHandle" << YAML::Value << c.MeshHandle;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& [direction, albedoColor, ambientColor, specularColor] = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Direction" << YAML::Value << direction;
			out << YAML::Key << "Albedo" << YAML::Value << albedoColor;
			out << YAML::Key << "Ambient" << YAML::Value << ambientColor;
			out << YAML::Key << "Specular" << YAML::Value << specularColor;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& [className] = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "ClassName" << YAML::Value << className;

			// Fields
			if (const auto entityClass = ScriptEngine::GetEntityClass(className))
			{
				if (const auto& fields = entityClass->GetFields();
					!fields.empty())
				{
					out << YAML::Key << "Fields" << YAML::Value;
					out << YAML::BeginSeq;

					auto& entityFields = ScriptEngine::GetScriptFieldMap(entity.GetUUID());
					for (const auto& [name, field] : fields)
					{
						if (entityFields.find(name) == entityFields.end())
							continue;

						out << YAML::BeginMap;
						out << YAML::Key << "Name" << YAML::Value << name;
						out << YAML::Key << "Type" << YAML::Value << Utils::ScriptFieldTypeToString(field.Type);
						out << YAML::Key << "Data" << YAML::Value;

						ScriptFieldInstance& scriptField = entityFields.at(name);

						switch (field.Type)
						{
							WRITE_SCRIPT_FIELD(Float, float);
							WRITE_SCRIPT_FIELD(Double, double);
							WRITE_SCRIPT_FIELD(Bool, bool);
							WRITE_SCRIPT_FIELD(Char, int8_t);
							WRITE_SCRIPT_FIELD(Int16, int16_t);
							WRITE_SCRIPT_FIELD(Int32, int32_t);
							WRITE_SCRIPT_FIELD(Int64, int64_t);
							WRITE_SCRIPT_FIELD(Byte, uint8_t);
							WRITE_SCRIPT_FIELD(UInt16, uint16_t);
							WRITE_SCRIPT_FIELD(UInt32, uint32_t);
							WRITE_SCRIPT_FIELD(UInt64, uint64_t);
							WRITE_SCRIPT_FIELD(Vector2, glm::vec2);
							WRITE_SCRIPT_FIELD(Vector3, glm::vec3);
							WRITE_SCRIPT_FIELD(Vector4, glm::vec4);
							WRITE_SCRIPT_FIELD(Entity, UUID);
						}
						out << YAML::EndMap;
					}
					out << YAML::EndSeq;
				}
			}
			out << YAML::EndMap;
		}
		
		if (entity.HasComponent<RigidBodyComponent>())
		{
			out << YAML::Key << "RigidBodyComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& c = entity.GetComponent<RigidBodyComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << static_cast<int>(c.Type);
			out << YAML::Key << "Mass" << YAML::Value << c.Mass;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& [camera] = entity.GetComponent<CameraComponent>();
			const auto& cc = camera;

			out << YAML::Key << "ProjectionType" << YAML::Value << static_cast<int>(cc.GetProjectionType());
			out << YAML::Key << "PerspectiveFov" << YAML::Value << cc.GetPerspectiveFov();
			out << YAML::Key << "PerspectiveNearClip" << YAML::Value << cc.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip" << YAML::Value << cc.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << cc.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip" << YAML::Value << cc.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip" << YAML::Value << cc.GetOrthographicFarClip();

			out << YAML::EndMap;
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& c = entity.GetComponent<PointLightComponent>();

			out << YAML::Key << "Color" << YAML::Value << c.Color;

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}
}
