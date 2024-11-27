#include "pch.h"
#include "SceneSerializer.h"

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

		EPPO_INFO("Serializing scene '{}'", sceneName);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << sceneName;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		
		m_SceneContext->m_Registry.each([&](auto entityID)
		{
			Entity entity(entityID, m_SceneContext.get());
			if (!entity)
				return;

			SerializeEntity(out, entity);
		});

		out << YAML::EndSeq << YAML::EndMap;

		Filesystem::WriteText(filepath, out.c_str());

		return true;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
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

		std::string name = data["Scene"].as<std::string>();
		EPPO_INFO("Deserializing scene '{}'", name);

		auto entities = data["Entities"];

		if (!entities)
		{
			EPPO_WARN("Scene '{}' has no entities, are you sure this is correct?", name);
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

			{
				auto c = entity["TransformComponent"];
				if (c)
				{
					auto& nc = newEntity.GetComponent<TransformComponent>();
					nc.Translation = c["Translation"].as<glm::vec3>();
					nc.Rotation = c["Rotation"].as<glm::vec3>();
					nc.Scale = c["Scale"].as<glm::vec3>();
				}
			}

			{
				auto c = entity["SpriteComponent"];
				if (c)
				{
					auto& nc = newEntity.AddComponent<SpriteComponent>();
					nc.Color = c["Color"].as<glm::vec4>();
					nc.TextureHandle = c["TextureHandle"].as<uint64_t>();
				}
			}

			{
				auto c = entity["MeshComponent"];
				if (c)
				{
					auto& nc = newEntity.AddComponent<MeshComponent>();
					nc.MeshHandle = c["MeshHandle"].as<uint64_t>();
				}
			}

			{
				auto c = entity["DirectionalLightComponent"];
				if (c)
				{
					auto& dlc = newEntity.AddComponent<DirectionalLightComponent>();
					dlc.Direction = c["Direction"].as<glm::vec3>();
					dlc.AlbedoColor = c["Albedo"].as<glm::vec4>();
					dlc.AmbientColor = c["Ambient"].as<glm::vec4>();
					dlc.SpecularColor = c["Specular"].as<glm::vec4>();
				}
			}

			{
				auto c = entity["ScriptComponent"];
				if (c)
				{
					auto& sc = newEntity.AddComponent<ScriptComponent>();
					sc.ClassName = c["ClassName"].as<std::string>();

					auto scriptFields = c["Fields"];
					if (scriptFields)
					{
						Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(sc.ClassName);
						EPPO_ASSERT(entityClass);

						const auto& fields = entityClass->GetFields();
						auto& entityFields = ScriptEngine::GetScriptFieldMap(uuid);

						for (auto scriptField : scriptFields)
						{
							std::string name = scriptField["Name"].as<std::string>();
							std::string typeString = scriptField["Type"].as<std::string>();
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
								READ_SCRIPT_FIELD(Float, float);
								READ_SCRIPT_FIELD(Double, double);
								READ_SCRIPT_FIELD(Bool, bool);
								READ_SCRIPT_FIELD(Char, int8_t);
								READ_SCRIPT_FIELD(Int16, int16_t);
								READ_SCRIPT_FIELD(Int32, int32_t);
								READ_SCRIPT_FIELD(Int64, int64_t);
								READ_SCRIPT_FIELD(Byte, uint8_t);
								READ_SCRIPT_FIELD(UInt16, uint16_t);
								READ_SCRIPT_FIELD(UInt32, uint32_t);
								READ_SCRIPT_FIELD(UInt64, uint64_t);
								READ_SCRIPT_FIELD(Vector2, glm::vec2);
								READ_SCRIPT_FIELD(Vector3, glm::vec3);
								READ_SCRIPT_FIELD(Vector4, glm::vec4);
								READ_SCRIPT_FIELD(Entity, UUID);
							}
						}
					}
				}
			}
			
			{
				auto c = entity["RigidBodyComponent"];
				if (c)
				{
					auto& rbc = newEntity.AddComponent<RigidBodyComponent>();
					rbc.Type = (RigidBodyComponent::BodyType)c["BodyType"].as<int>();
					rbc.Mass = c["Mass"].as<float>();
				}
			}

			{
				auto c = entity["CameraComponent"];
				if (c)
				{
					auto& cc = newEntity.AddComponent<CameraComponent>();
					cc.Camera.SetProjectionType((ProjectionType)c["ProjectionType"].as<int>());
					cc.Camera.SetPerspectiveFov(c["PerspectiveFov"].as<float>());
					cc.Camera.SetPerspectiveNearClip(c["PerspectiveNearClip"].as<float>());
					cc.Camera.SetPerspectiveFarClip(c["PerspectiveFarClip"].as<float>());
					cc.Camera.SetOrthographicSize(c["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(c["OrthographicNearClip"].as<float>());
					cc.Camera.SetOrthographicFarClip(c["OrthographicFarClip"].as<float>());
				}
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

			auto& c = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << c.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << c.Scale;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			out << YAML::Key << "SpriteComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto& c = entity.GetComponent<SpriteComponent>();
			out << YAML::Key << "Color" << YAML::Value << c.Color;
			out << YAML::Key << "TextureHandle" << YAML::Value << c.TextureHandle;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto& c = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "MeshHandle" << YAML::Value << c.MeshHandle;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto& c = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Direction" << YAML::Value << c.Direction;
			out << YAML::Key << "Albedo" << YAML::Value << c.AlbedoColor;
			out << YAML::Key << "Ambient" << YAML::Value << c.AmbientColor;
			out << YAML::Key << "Specular" << YAML::Value << c.SpecularColor;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto& c = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "ClassName" << YAML::Value << c.ClassName;

			// Fields
			Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(c.ClassName);

			if (entityClass)
			{
				const auto& fields = entityClass->GetFields();

				if (!fields.empty())
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
			out << YAML::Key << "BodyType" << YAML::Value << (int)c.Type;
			out << YAML::Key << "Mass" << YAML::Value << c.Mass;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent" << YAML::Value;
			out << YAML::BeginMap;

			const auto& c = entity.GetComponent<CameraComponent>();
			const auto& cc = c.Camera;

			out << YAML::Key << "ProjectionType" << YAML::Value << (int)cc.GetProjectionType();
			out << YAML::Key << "PerspectiveFov" << YAML::Value << cc.GetPerspectiveFov();
			out << YAML::Key << "PerspectiveNearClip" << YAML::Value << cc.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip" << YAML::Value << cc.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << cc.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip" << YAML::Value << cc.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip" << YAML::Value << cc.GetOrthographicFarClip();

			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}
}
