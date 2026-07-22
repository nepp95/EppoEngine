#pragma once

#include "Project/Project.h"

namespace Eppo
{
	using ProjectExportProgressCallback = std::function<void(float, std::string_view)>;

	struct ProjectExportOptions
	{
		std::filesystem::path ParentDirectory;
		std::filesystem::path SourceDirectory;
		std::filesystem::path DebugRuntimeDirectory;
		std::filesystem::path ReleaseRuntimeDirectory;
		std::filesystem::path DebugManagedDirectory;
		std::filesystem::path ReleaseManagedDirectory;
		bool ExportDebug = true;
		bool ExportRelease = true;
		bool BuildRuntime = true;
		bool BuildScripts = true;
		bool CopyRuntime = true;
		ProjectExportProgressCallback ProgressCallback;
	};

	struct ProjectExportResult
	{
		bool Success = false;
		std::filesystem::path OutputPath;
		std::vector<std::string> Warnings;
		std::vector<std::string> Errors;
	};

	class ProjectExporter
	{
	public:
		explicit ProjectExporter(Ref<Project> project);

		[[nodiscard]] auto Export(const ProjectExportOptions& options) const -> ProjectExportResult;

	private:
		Ref<Project> m_Project = nullptr;
	};
}
