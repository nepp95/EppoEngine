#include "pch.h"
#include "ProjectSerializer.h"

#include <yaml-cpp/yaml.h>

namespace Eppo
{
	ProjectSerializer::ProjectSerializer(Ref<Project> project)
		: m_Project(project)
	{

	}

	bool ProjectSerializer::Serialize()
	{
		EPPO_PROFILE_FUNCTION("ProjectSerializer::Serialize");

		const auto& spec = m_Project->GetSpecification();

		YAML::Emitter out;

		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;

		out << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << spec.Name;
		out << YAML::Key << "ProjectDirectory" << YAML::Value << spec.ProjectDirectory.string();
		out << YAML::Key << "StartScene" << YAML::Value << spec.StartScene.string();
		out << YAML::EndMap;

		out << YAML::EndMap;

		std::ofstream fout(spec.ProjectDirectory / std::filesystem::path(spec.Name + ".epproj"));
		fout << out.c_str();

		return true;
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		EPPO_PROFILE_FUNCTION("ProjectSerializer::Deserialize");

		auto& spec = m_Project->GetSpecification();

		YAML::Node data;

		try
		{
			data = YAML::LoadFile(filepath.string());
		} catch (YAML::ParserException& e)
		{
			EPPO_ERROR("Failed to load project file '{}'!\nError: {}", filepath, e.what());
			return false;
		}

		auto projectNode = data["Project"];
		if (!projectNode)
			return false;

		spec.Name = projectNode["Name"].as<std::string>();
		EPPO_INFO("Deserializing project '{}'", spec.Name);

		if (projectNode["ProjectDirectory"])
			spec.ProjectDirectory = std::filesystem::path(projectNode["ProjectDirectory"].as<std::string>());

		if (projectNode["StartScene"])
			spec.StartScene = projectNode["StartScene"].as<std::string>();

		return true;
	}
}
