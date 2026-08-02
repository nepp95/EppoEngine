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
        struct ExportConfiguration
        {
            std::string Name;
            std::string CMakePreset;
            std::string ScriptConfiguration;
            std::filesystem::path RuntimeDirectory;
            std::filesystem::path ManagedDirectory;
            bool IncludeDebugSymbols = false;
        };

        auto ValidateProject(const ProjectExportOptions& options, ProjectExportResult& result) const -> void;
        auto CopyAssets(const std::filesystem::path& source, const std::filesystem::path& destination) const -> bool;
        auto ReportProgress(const ProjectExportOptions& options, float value, std::string_view phase) const -> void;
        auto GetExportConfigurations(const ProjectExportOptions& options) const -> std::vector<ExportConfiguration>;
        auto BuildRuntime(
            const ProjectExportOptions& options, const std::filesystem::path& sourceDirectory, const ExportConfiguration& configuration,
            float progressStart, float progressSpan, std::string& error
        ) const -> bool;
        auto ValidateRuntimeFiles(const ExportConfiguration& configuration, std::string& error) const -> bool;

    private:
        Ref<Project> m_Project = nullptr;
    };
}
