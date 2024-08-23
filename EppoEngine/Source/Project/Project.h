#pragma once

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

		static const std::filesystem::path& GetProjectDirectory();
		static std::filesystem::path GetProjectFile();
		static std::filesystem::path GetAssetsDirectory();
		static std::filesystem::path GetAssetFilepath(const std::filesystem::path& filepath);

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(Ref<Project> project) { s_ActiveProject = project; }

		static Ref<Project> New();
		static Ref<Project> New(const ProjectSpecification& specification);
		static Ref<Project> Open(const std::filesystem::path& filepath);
		static bool SaveActive(const std::filesystem::path& filepath);

	private:
		ProjectSpecification m_Specification;

		inline static Ref<Project> s_ActiveProject;
	};
}
