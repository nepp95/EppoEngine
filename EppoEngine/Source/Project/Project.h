#pragma once

#include "Asset/AssetManagerEditor.h"

namespace Eppo
{
	struct ProjectSpecification
	{
		std::string Name = "Untitled";

		std::filesystem::path StartScene;
		std::filesystem::path ProjectDirectory;
	};

	class Project
	{
	public:
		ProjectSpecification& GetSpecification() { return m_Specification; }
		Ref<AssetManagerBase> GetAssetManager() { return m_AssetManager; }
		Ref<AssetManagerEditor> GetAssetManagerEditor() { return std::static_pointer_cast<AssetManagerEditor>(m_AssetManager); }

		static const std::filesystem::path& GetProjectDirectory();
		static std::filesystem::path GetProjectsDirectory();
		static std::filesystem::path GetProjectFile();
		static std::filesystem::path GetAssetsDirectory();
		static std::filesystem::path GetAssetFilepath(const std::filesystem::path& filepath);
		static std::filesystem::path GetAssetRelativeFilepath(const std::filesystem::path& filepath);

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(Ref<Project> project) { s_ActiveProject = project; }

		static Ref<Project> New();
		static Ref<Project> New(const ProjectSpecification& specification);
		static Ref<Project> Open(const std::filesystem::path& filepath);
		static bool SaveActive();

	private:
		ProjectSpecification m_Specification;
		Ref<AssetManagerBase> m_AssetManager;

		inline static Ref<Project> s_ActiveProject;
	};
}
