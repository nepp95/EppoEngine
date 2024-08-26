#include "pch.h"
#include "Project.h"

#include "Project/ProjectSerializer.h"

namespace Eppo
{
	const std::filesystem::path& Project::GetProjectDirectory()
	{
		EPPO_ASSERT(s_ActiveProject);
		return s_ActiveProject->m_Specification.ProjectDirectory;
	}

	std::filesystem::path Project::GetProjectFile()
	{
		EPPO_ASSERT(s_ActiveProject);
		return GetProjectDirectory() / std::filesystem::path(s_ActiveProject->m_Specification.Name + ".epproj");
	}

	std::filesystem::path Project::GetAssetsDirectory()
	{
		EPPO_ASSERT(s_ActiveProject);
		return GetProjectDirectory() / "Assets";
	}

	std::filesystem::path Project::GetAssetFilepath(const std::filesystem::path& filepath)
	{
		EPPO_ASSERT(s_ActiveProject);
		return GetAssetsDirectory() / filepath;
	}

	std::filesystem::path Project::GetAssetRelativeFilepath(const std::filesystem::path& filepath)
	{
		EPPO_ASSERT(s_ActiveProject);
		return std::filesystem::relative(filepath, GetAssetsDirectory());
	}

	Ref<Project> Project::New()
	{
		s_ActiveProject = CreateRef<Project>();
		return s_ActiveProject;
	}

	Ref<Project> Project::New(const ProjectSpecification& specification)
	{
		New();
		s_ActiveProject->m_Specification = specification;
		return s_ActiveProject;
	}

	Ref<Project> Project::Open(const std::filesystem::path& filepath)
	{
		Ref<Project> project = CreateRef<Project>();

		ProjectSerializer serializer(project);
		if (serializer.Deserialize(filepath))
		{
			project->GetSpecification().ProjectDirectory = filepath.parent_path();
			s_ActiveProject = project;

			Ref<AssetManagerEditor> assetManager = CreateRef<AssetManagerEditor>();
			s_ActiveProject->m_AssetManager = assetManager;
			assetManager->DeserializeAssetRegistry();

			return s_ActiveProject;
		}

		return nullptr;
	}

	bool Project::SaveActive()
	{
		ProjectSerializer serializer(s_ActiveProject);
		return serializer.Serialize();
	}
}
