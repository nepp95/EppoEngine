#pragma once

#include "Asset/AssetManager.h"

namespace Eppo
{
	struct ProjectSpecification
	{
		std::string Name = "Untitled Project";
		std::filesystem::path ProjectDirectory;
		AssetHandle StartScene = 0;
	};

	class Project
	{
	public:
		[[nodiscard]] auto GetSpecification() -> ProjectSpecification& { return m_Specification; }
		[[nodiscard]] auto GetSpecification() const -> const ProjectSpecification& { return m_Specification; }
		[[nodiscard]] auto GetAssetManager() const -> const Ref<AssetManager>& { return m_AssetManager; }

		static auto GetActive() -> Ref<Project> { return s_ActiveProject; }
		static auto SetActive(const Ref<Project>& project) -> void { s_ActiveProject = project; }

		static auto New() -> Ref<Project>;
		static auto New(const ProjectSpecification& spec) -> Ref<Project>;
		static auto Open(const std::filesystem::path& path) -> Ref<Project>;
		static auto SaveActive() -> bool;

		static auto GetProjectDirectory() -> const std::filesystem::path&;
		static auto GetProjectsDirectory() -> std::filesystem::path;
		static auto GetProjectFile() -> std::filesystem::path;
		static auto GetAssetsDirectory() -> std::filesystem::path;
		static auto GetScriptsDirectory() -> std::filesystem::path;
		static auto GetAssetFilepath(const std::filesystem::path& filepath) -> std::filesystem::path;
		static auto GetAssetRelativeFilepath(const std::filesystem::path& filepath) -> std::filesystem::path;

	private:
		ProjectSpecification m_Specification;
		Ref<AssetManager> m_AssetManager = nullptr;

		inline static Ref<Project> s_ActiveProject = nullptr;
	};
}