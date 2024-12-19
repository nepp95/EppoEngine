#pragma once

#include "Asset/AssetManagerEditor.h"

namespace Eppo
{
	struct ProjectSpecification
	{
		std::string Name = "Untitled";

		AssetHandle StartScene = 0;
		std::filesystem::path ProjectDirectory;
	};

	class Project
	{
	public:
		ProjectSpecification& GetSpecification() { return m_Specification; }
		[[nodiscard]] Ref<AssetManagerBase> GetAssetManager() const { return m_AssetManager; }
		[[nodiscard]] Ref<AssetManagerEditor> GetAssetManagerEditor() const { return std::static_pointer_cast<AssetManagerEditor>(m_AssetManager); }

		static const std::filesystem::path& GetProjectDirectory();
		static std::filesystem::path GetProjectsDirectory();
		static std::filesystem::path GetProjectFile();
		static std::filesystem::path GetAssetsDirectory();
		static std::filesystem::path GetAssetFilepath(const std::filesystem::path& filepath);
		static std::filesystem::path GetAssetRelativeFilepath(const std::filesystem::path& filepath);

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(const Ref<Project>& project) { s_ActiveProject = project; }

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
