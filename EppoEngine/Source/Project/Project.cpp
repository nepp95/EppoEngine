#include "pch.h"
#include "Project/Project.h"

#include "Project/ProjectSerializer.h"
#include "Scene/SceneSerializer.h"

namespace Eppo
{
	auto Project::New() -> Ref<Project>
	{
		s_ActiveProject = CreateRef<Project>();
		return s_ActiveProject;
	}

	auto Project::New(const ProjectSpecification& spec) -> Ref<Project>
	{
		New();
		s_ActiveProject->m_Specification = spec;
		return s_ActiveProject;
	}

	auto Project::Open(const std::filesystem::path& path) -> Ref<Project>
	{
		EP_PROFILE_FN("Project::Open");

		const auto project = CreateRef<Project>();

		if (ProjectSerializer serializer(project); !serializer.Deserialize(path))
			return nullptr;

		project->GetSpecification().ProjectDirectory = path.parent_path();
		s_ActiveProject = project;

		const auto assetManager = CreateRef<AssetManager>();
		s_ActiveProject->m_AssetManager = assetManager;
		assetManager->DeserializeAssetRegistry();

		return s_ActiveProject;
	}

	auto Project::SaveActive() -> bool
	{
		EP_PROFILE_FN("Project::SaveActive");

		const auto& assetManager = s_ActiveProject->GetAssetManager();
		const auto& registry = assetManager->GetAssetRegistry();
		
		for (const auto& [handle, metadata] : registry)
		{
			if (metadata.Type != AssetType::Scene)
				continue;

			Ref<Scene> scene = assetManager->GetOrLoadAsset<Scene>(handle);

			SceneSerializer serializer(scene);
			serializer.Serialize(GetAssetFilepath(metadata.Filepath));
		}

		assetManager->SerializeAssetRegistry();

		ProjectSerializer serializer(s_ActiveProject);
		return serializer.Serialize();
	}

	auto Project::GetProjectDirectory() -> const std::filesystem::path&
	{
		EP_ASSERT(s_ActiveProject != nullptr);
		return s_ActiveProject->m_Specification.ProjectDirectory;
	}

	auto Project::GetProjectsDirectory() -> std::filesystem::path
	{
		return FS::GetRootDirectory() / "Projects";
	}

	auto Project::GetProjectFile() -> std::filesystem::path
	{
		EP_ASSERT(s_ActiveProject != nullptr);
		return GetProjectDirectory() / std::filesystem::path(s_ActiveProject->m_Specification.Name + ".epproj");
	}

	auto Project::GetAssetsDirectory() -> std::filesystem::path
	{
		EP_ASSERT(s_ActiveProject != nullptr);
		return GetProjectDirectory() / "Assets";
	}

	auto Project::GetScriptsDirectory() -> std::filesystem::path
	{
		return GetProjectDirectory() / "Scripts";
	}

	auto Project::GetAssetFilepath(const std::filesystem::path& filepath) -> std::filesystem::path
	{
		EP_ASSERT(s_ActiveProject != nullptr);
		return GetAssetsDirectory() / filepath;
	}

	auto Project::GetAssetRelativeFilepath(const std::filesystem::path& filepath) -> std::filesystem::path
	{
		EP_ASSERT(s_ActiveProject != nullptr);
		return std::filesystem::relative(filepath, GetAssetsDirectory());
	}
}