#include "pch.h"
#include "SceneSerializer.h"

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
}

namespace Eppo
{
	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_SceneContext(scene)
	{}

	bool SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("SceneSerializer::Serialize");

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

		std::ofstream fout(filepath);
		fout << out.c_str();

		return true;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("SceneSerializer::Deserialize");

		YAML::Node data;

		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (YAML::ParserException e)
		{
			EPPO_ERROR("Failed to load scene file '{}'!", filepath.string());
			EPPO_ERROR("YAML Error: {}", e.what());
			return false;
		}

		if (!data["Scene"])
		{
			EPPO_ERROR("Failed to load scene file '{}'! Not a scene file!", filepath.string());
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
			std::string name;

			auto tagComponent = entity["TagComponent"];
			if (tagComponent)
				name = tagComponent["Tag"].as<std::string>();

			Entity newEntity = m_SceneContext->CreateEntityWithUUID(uuid, name);
			EPPO_INFO("Deserializing entity '{}' ({})", name, uuid);

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
				auto c = entity["ColorComponent"];
				if (c)
				{
					auto& nc = newEntity.AddComponent<ColorComponent>();
					nc.Color = c["Color"].as<glm::vec4>();
				}
			}
		}

		return true;
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		EPPO_PROFILE_FUNCTION("SceneSerializer::SerializeEntity");
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

		if (entity.HasComponent<ColorComponent>())
		{
			out << YAML::Key << "ColorComponent" << YAML::Value;
			out << YAML::BeginMap;

			auto& c = entity.GetComponent<ColorComponent>();
			out << YAML::Key << "Color" << YAML::Value << c.Color;
			
			out << YAML::EndMap;
		}

		out << YAML::EndMap;
	}
}