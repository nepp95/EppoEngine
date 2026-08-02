#include "pch.h"
#include "Project/ProjectExporter.h"

#include "Project/GameData.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"
#include "Utility/Process.h"

namespace Eppo
{
    namespace
    {
        auto RuntimeExecutableName() -> std::filesystem::path
        {
#if defined(EP_PLATFORM_WINDOWS)
            return "EppoRuntime.exe";
#elif defined(EP_PLATFORM_LINUX)
            return "EppoRuntime";
#endif
        }
    }

    ProjectExporter::ProjectExporter(Ref<Project> project)
        : m_Project(std::move(project))
    {}

    auto ProjectExporter::Export(const ProjectExportOptions& options) const -> ProjectExportResult
    {
        EP_PROFILE_FN("ProjectExporter::Export");

        ProjectExportResult result;

        // Validate the project, name, configurations, and start scene before touching the filesystem.
        ReportProgress(options, 0.02f, "Validating project");
        ValidateProject(options, result);
        if (!result.Errors.empty())
            return result;

        const auto& [projectName, projectDirectory, startScene] = m_Project->GetSpecification();

        GameData gameData;
        gameData.ProjectName = projectName;
        gameData.StartScene = startScene;
        gameData.AssetRegistry = m_Project->GetAssetManager()->GetAssetRegistry();

        // Pack scenes: a packed scene is the .epscene file's bytes carried inside Game.eppak.
        ReportProgress(options, 0.05f, "Packing scenes");

        const size_t sceneCount = std::ranges::count_if(
            gameData.AssetRegistry,
            [](const auto& entry)
            {
                return entry.second.Type == AssetType::Scene && !entry.second.IsRuntimeAsset;
            }
        );
        size_t packedSceneCount = 0;

        for (const auto& [handle, metadata] : gameData.AssetRegistry)
        {
            if (metadata.Type != AssetType::Scene || metadata.IsRuntimeAsset)
                continue;

            const auto sourcePath = projectDirectory / "Assets" / metadata.Filepath;
            const auto bytes = FS::ReadBytes(sourcePath);
            if (bytes.empty())
            {
                result.Errors.emplace_back(std::format("Scene '{}' could not be read.", metadata.Filepath.generic_string()));
                return result;
            }

            PackedAssetData packedAsset{ .Type = AssetType::Scene };
            packedAsset.Payload = Buffer::Copy(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
            gameData.PackedAssets.emplace(handle, std::move(packedAsset));

            ++packedSceneCount;
            ReportProgress(
                options, 0.05f + 0.10f * static_cast<float>(packedSceneCount) / static_cast<float>(sceneCount),
                std::format("Packed scene {} of {}", packedSceneCount, sceneCount)
            );
        }

        // The engine shaders are already compiled in memory, so pack their sources.
        ReportProgress(options, 0.15f, "Packing shaders");
        for (const auto& [name, shader] : DeviceManager::Get()->GetRenderer()->GetAllShaders())
            gameData.PackedShaders.emplace(name, shader->GetShaderSource());

        // Those sources #include by path relative to Resources/Shaders, so key the packed copies the same way.
        const auto shadersDirectory = FS::GetResourcesDirectory() / "Shaders";
        if (!FS::Exists(shadersDirectory))
        {
            result.Errors.emplace_back("Engine shader resources are missing; the packaged game would have no shaders.");
            return result;
        }

        // Kept in step with ReadIncludesFromDisk in VulkanShader.cpp, which hashes the same set for the cache key.
        for (const auto& entry : std::filesystem::recursive_directory_iterator(shadersDirectory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".hlsli")
                continue;

            const auto relativePath = std::filesystem::relative(entry.path(), shadersDirectory).generic_string();
            auto source = FS::ReadText(entry.path());
            if (source.empty())
            {
                result.Errors.emplace_back(std::format("Shader include '{}' could not be read.", relativePath));
                return result;
            }

            gameData.PackedShaderIncludes.emplace(relativePath, std::move(source));
        }

        // Build (optional) and validate the standalone runtime for each requested configuration.
        const auto configurations = GetExportConfigurations(options);
        if (options.CopyRuntime)
        {
            for (size_t index = 0; index < configurations.size(); ++index)
            {
                const auto& configuration = configurations[index];
                const float progressStart = 0.20f + 0.32f * static_cast<float>(index) / static_cast<float>(configurations.size());
                const float progressSpan = 0.32f / static_cast<float>(configurations.size());
                std::string runtimeError;

                if (options.BuildRuntime &&
                    !BuildRuntime(options, options.SourceDirectory, configuration, progressStart, progressSpan, runtimeError))
                {
                    result.Errors.emplace_back(std::move(runtimeError));
                    return result;
                }

                if (!ValidateRuntimeFiles(configuration, runtimeError))
                {
                    result.Errors.emplace_back(std::move(runtimeError));
                    return result;
                }

                ReportProgress(options, progressStart + progressSpan, std::format("Validated {} runtime", configuration.Name));
            }
        }

        // Lay down the output tree. From here failures wipe the partial export via fail().
        ReportProgress(options, 0.54f, "Preparing export directories");

        if (!FS::Exists(result.OutputPath) && !FS::CreateDir(result.OutputPath))
        {
            result.Errors.emplace_back("Failed to create the export target directory.");
            return result;
        }

        const auto fail = [&](std::string message) -> ProjectExportResult
        {
            result.Errors.emplace_back(std::move(message));
            FS::RemoveAll(result.OutputPath);
            return result;
        };

        const auto scriptsDirectory = projectDirectory / "Scripts";
        const auto scriptProject = scriptsDirectory / (projectName + ".csproj");
        if (!options.BuildScripts)
        {
            result.Warnings.emplace_back("Script build was skipped.");
        }
        if (!options.CopyRuntime)
        {
            result.Warnings.emplace_back("Runtime files were skipped.");
        }

        for (size_t index = 0; index < configurations.size(); ++index)
        {
            const auto& configuration = configurations[index];
            const float progressStart = 0.56f + 0.40f * static_cast<float>(index) / static_cast<float>(configurations.size());
            const float progressSpan = 0.40f / static_cast<float>(configurations.size());
            const auto outputDirectory = result.OutputPath / configuration.Name;
            if (!FS::CreateDir(outputDirectory))
                return fail(std::format("Failed to create the {} export directory.", configuration.Name));

            // Compile the user's C# scripts into this configuration's output.
            if (options.BuildScripts && FS::Exists(scriptProject))
            {
                ReportProgress(options, progressStart, std::format("Compiling {} scripts", configuration.Name));
                std::vector<std::string> args{
                    "build",
                    scriptProject.string(),
                    "-c",
                    configuration.ScriptConfiguration,
                    "-o",
                    outputDirectory.string(),
                    "-p:AssemblyName=" + gameData.ProjectName,
                    "-p:CoreManagedDll=" + (configuration.ManagedDirectory / "EppoScriptCore.dll").string(),
                    "--nologo",
                };
                if (!configuration.IncludeDebugSymbols)
                {
                    args.emplace_back("-p:DebugSymbols=false");
                    args.emplace_back("-p:DebugType=None");
                }
                const int32_t exitCode = RunProcess("dotnet", args);
                if (exitCode != 0)
                    return fail(std::format("{} user script build failed with exit code {}.", configuration.Name, exitCode));
            }

            // Stage the runtime executable, its native dependencies, the managed core, and engine resources.
            if (options.CopyRuntime)
            {
                ReportProgress(options, progressStart + progressSpan * 0.25f, std::format("Staging {} runtime", configuration.Name));
                const auto exportedExecutable = outputDirectory / (gameData.ProjectName + RuntimeExecutableName().extension().string());
                if (!FS::CopyFile(configuration.RuntimeDirectory / RuntimeExecutableName(), exportedExecutable, true))
                    return fail(std::format("Failed to copy the {} runtime executable.", configuration.Name));

                // Native deps live beside the runtime; the exe and managed core are copied separately below.
                const bool includeSymbols = configuration.IncludeDebugSymbols;
                if (!FS::CopyDirectory(
                        configuration.RuntimeDirectory, outputDirectory,
                        [&](const std::filesystem::path& path)
                        {
                            const auto extension = path.extension();
                            const auto filename = path.filename();
                            const bool dependency = extension == ".dll" || filename == "runtimeconfig.json" ||
                                filename.string().find(".so") != std::string::npos || (includeSymbols && extension == ".pdb");
                            return dependency && filename != RuntimeExecutableName() && !filename.string().starts_with("EppoScriptCore.");
                        },
                        false
                    ))
                    return fail(std::format("Failed to copy {} runtime dependencies.", configuration.Name));

                for (const auto* filename : { "EppoScriptCore.dll", "EppoScriptCore.deps.json" })
                {
                    if (!FS::CopyFile(configuration.ManagedDirectory / filename, outputDirectory / filename, true))
                        return fail(std::format("Failed to copy managed runtime dependency '{}'.", filename));
                }
                if (configuration.IncludeDebugSymbols &&
                    !FS::CopyFile(configuration.ManagedDirectory / "EppoScriptCore.pdb", outputDirectory / "EppoScriptCore.pdb", true))
                    return fail("Failed to copy managed runtime debug symbols.");
            }

            // Copy the loose project assets (Game.eppak carries the scenes and registry).
            const auto assetsDirectory = projectDirectory / "Assets";
            ReportProgress(options, progressStart + progressSpan * 0.70f, std::format("Copying {} assets", configuration.Name));
            if (FS::Exists(assetsDirectory) && !CopyAssets(assetsDirectory, outputDirectory / "Assets"))
                return fail("Failed to copy project assets.");

            // Write the packed container that the runtime loads at startup.
            ReportProgress(options, progressStart + progressSpan * 0.90f, std::format("Writing {} game data", configuration.Name));
            if (!gameData.Serialize(outputDirectory / GameData::Filename))
                return fail("Failed to write Game.eppak.");
            ReportProgress(options, progressStart + progressSpan, std::format("Packaged {}", configuration.Name));
        }

        result.Success = true;
        ReportProgress(options, 1.0f, "Export complete");
        return result;
    }

    auto ProjectExporter::ValidateProject(const ProjectExportOptions& options, ProjectExportResult& result) const -> void
    {
        EP_PROFILE_FN("ProjectExporter::ValidateProject");

        if (!m_Project || !m_Project->GetAssetManager())
        {
            result.Errors.emplace_back("No valid project is available for export.");
            return;
        }

        const auto& specification = m_Project->GetSpecification();
        if (specification.Name.empty())
            result.Errors.emplace_back("Project name cannot be empty.");

        if (const auto configurations = GetExportConfigurations(options); configurations.empty())
            result.Errors.emplace_back("At least one export configuration must be selected.");

        if (options.CopyRuntime && options.BuildRuntime && options.SourceDirectory.empty())
            result.Errors.emplace_back("The engine source directory is required when building the runtime.");

        result.OutputPath = (options.ParentDirectory / specification.Name).lexically_normal();
        if (FS::Exists(result.OutputPath))
        {
            if (!FS::IsDirectory(result.OutputPath))
                result.Errors.emplace_back("Export target exists and is not a directory.");
            else if (!FS::IsEmpty(result.OutputPath))
                result.Errors.emplace_back("Export target directory is not empty.");
        }

        if (!specification.StartScene)
            result.Errors.emplace_back("A start scene is not configured.");

        const auto& registry = m_Project->GetAssetManager()->GetAssetRegistry();
        const auto startSceneMetadata = registry.find(specification.StartScene);
        if (startSceneMetadata == registry.end())
            result.Errors.emplace_back("The configured start scene is not registered.");
        else if (startSceneMetadata->second.Type != AssetType::Scene)
            result.Errors.emplace_back("The configured start scene is not a scene asset.");
    }

    auto ProjectExporter::CopyAssets(const std::filesystem::path& source, const std::filesystem::path& destination) const -> bool
    {
        // Ship the raw asset tree but drop editor-only bookkeeping: the registry and source scenes live in Game.eppak.
        return FS::CopyDirectory(
            source, destination,
            [](const std::filesystem::path& path)
            {
                return path.filename() != "AssetRegistry.json" && path.extension() != ".epscene";
            }
        );
    }

    auto ProjectExporter::ReportProgress(const ProjectExportOptions& options, const float value, const std::string_view phase) const -> void
    {
        if (options.ProgressCallback)
            options.ProgressCallback(std::clamp(value, 0.0f, 1.0f), phase);
    }

    auto ProjectExporter::GetExportConfigurations(const ProjectExportOptions& options) const -> std::vector<ExportConfiguration>
    {
        std::vector<ExportConfiguration> configurations;

        if (options.ExportDebug)
        {
            const auto runtimeDirectory =
                options.DebugRuntimeDirectory.empty() ? options.SourceDirectory / "build" / "debug" / "EppoRuntime" : options.DebugRuntimeDirectory;
            configurations.emplace_back(
                ExportConfiguration{
                    .Name = "Debug",
#if defined(EP_PLATFORM_WINDOWS)
                    .CMakePreset = "windows-debug",
#else
                    .CMakePreset = "linux-debug",
#endif
                    .ScriptConfiguration = "Debug",
                    .RuntimeDirectory = runtimeDirectory,
                    .ManagedDirectory =
                        options.DebugManagedDirectory.empty() ? runtimeDirectory.parent_path() : options.DebugManagedDirectory,
                    .IncludeDebugSymbols = true,
                }
            );
        }

        if (options.ExportRelease)
        {
            const auto runtimeDirectory = options.ReleaseRuntimeDirectory.empty()
                ? options.SourceDirectory / "build" / "dist" / "EppoRuntime"
                : options.ReleaseRuntimeDirectory;
            configurations.emplace_back(
                ExportConfiguration{
                    .Name = "Release",
#if defined(EP_PLATFORM_WINDOWS)
                    .CMakePreset = "windows-dist",
#else
                    .CMakePreset = "linux-dist",
#endif
                    .ScriptConfiguration = "Release",
                    .RuntimeDirectory = runtimeDirectory,
                    .ManagedDirectory =
                        options.ReleaseManagedDirectory.empty() ? runtimeDirectory.parent_path() : options.ReleaseManagedDirectory,
                    .IncludeDebugSymbols = false,
                }
            );
        }

        return configurations;
    }

    auto ProjectExporter::BuildRuntime(
        const ProjectExportOptions& options, const std::filesystem::path& sourceDirectory, const ExportConfiguration& configuration,
        const float progressStart, const float progressSpan, std::string& error
    ) const -> bool
    {
        if (!FS::Exists(sourceDirectory / "CMakePresets.json"))
        {
            error = std::format("CMakePresets.json was not found in '{}'.", sourceDirectory.string());
            return false;
        }

        ReportProgress(options, progressStart, std::format("Configuring {} runtime", configuration.Name));
        int32_t exitCode = RunProcess("cmake", { "-E", "chdir", sourceDirectory.string(), "cmake", "--preset", configuration.CMakePreset });

        if (exitCode != 0)
        {
            error = std::format("{} runtime configuration failed with exit code {}.", configuration.Name, exitCode);
            return false;
        }

        ReportProgress(options, progressStart + progressSpan * 0.25f, std::format("Building {} runtime", configuration.Name));
        exitCode = RunProcess(
            "cmake",
            { "-E", "chdir", sourceDirectory.string(), "cmake", "--build", "--preset", configuration.CMakePreset, "--target",
              "EppoRuntime" }
        );

        if (exitCode != 0)
        {
            error = std::format("{} runtime build failed with exit code {}.", configuration.Name, exitCode);
            return false;
        }

        ReportProgress(options, progressStart + progressSpan, std::format("Built {} runtime", configuration.Name));
        return true;
    }

    auto ProjectExporter::ValidateRuntimeFiles(const ExportConfiguration& configuration, std::string& error) const -> bool
    {
        std::vector<std::filesystem::path> required{
            RuntimeExecutableName(),
            "runtimeconfig.json",
        };

#if defined(EP_PLATFORM_WINDOWS)
        required.emplace_back("dxcompiler.dll");
        if (configuration.IncludeDebugSymbols)
            required.emplace_back("EppoRuntime.pdb");
#endif

        for (const auto& path : required)
        {
            if (!FS::Exists(configuration.RuntimeDirectory / path))
            {
                error = std::format("Required runtime file '{}' is missing.", (configuration.RuntimeDirectory / path).string());
                return false;
            }
        }

        for (const auto* filename : { "EppoScriptCore.dll", "EppoScriptCore.deps.json" })
        {
            if (!FS::Exists(configuration.ManagedDirectory / filename))
            {
                error = std::format("Required managed runtime file '{}' is missing.", (configuration.ManagedDirectory / filename).string());
                return false;
            }
        }

        if (configuration.IncludeDebugSymbols && !FS::Exists(configuration.ManagedDirectory / "EppoScriptCore.pdb"))
        {
            error = std::format(
                "Required managed debug symbols '{}' are missing.", (configuration.ManagedDirectory / "EppoScriptCore.pdb").string()
            );
            return false;
        }

        return true;
    }
}
