#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/TempDir.h"

#include "Project/GameData.h"
#include "Project/Project.h"
#include "Project/ProjectExporter.h"
#include "Scene/Entity.h"
#include "Scene/SceneSerializer.h"

#include <ranges>

using namespace Eppo;

// Export reads the engine shaders from the live renderer, so these boot the graphical harness.
namespace
{
    class ExportProjectFixture
    {
    public:
        explicit ExportProjectFixture(std::string name = "ExportGame")
            : PreviousProject(Project::GetActive()),
              ProjectDirectory(Directory.File("Project")),
              AssetManagerInstance(CreateRef<AssetManager>())
        {
            std::filesystem::create_directories(ProjectDirectory / "Assets" / "Scenes");
            ProjectInstance = Project::New(
                ProjectSpecification{
                    .Name = std::move(name),
                    .ProjectDirectory = ProjectDirectory,
                },
                AssetManagerInstance
            );
        }

        ~ExportProjectFixture() { Project::SetActive(PreviousProject); }

        auto AddScene(const uint64_t handle, const std::string& filename, const bool primaryCamera = true) -> Ref<Scene>
        {
            const Ref<Scene> scene = CreateRef<Scene>();
            scene->Handle = AssetHandle(handle);
            Entity camera = scene->CreateEntity("Camera");
            if (primaryCamera)
                camera.AddComponent<CameraComponent>();

            const auto relativePath = std::filesystem::path("Scenes") / filename;
            const auto path = ProjectDirectory / "Assets" / relativePath;
            EXPECT_TRUE(SceneSerializer(scene).Serialize(path));
            EXPECT_TRUE(AssetManagerInstance->CreateAsset(path, scene));
            return scene;
        }

        auto AddTexture(const uint64_t handle, const std::string& filename = "texture.png") -> std::filesystem::path
        {
            const auto path = ProjectDirectory / "Assets" / filename;
            const std::vector<char> bytes{ 't', 'e', 'x' };
            EXPECT_TRUE(FS::WriteBytes(path, bytes, true));
            const Ref<Asset> asset = CreateRef<Asset>();
            asset->Handle = AssetHandle(handle);
            EXPECT_TRUE(AssetManagerInstance->CreateAsset(path, asset));
            return path;
        }

        auto Options() const -> ProjectExportOptions
        {
            return {
                .ParentDirectory = Directory.File("Exports"),
                .ExportRelease = false,
                .BuildRuntime = false,
                .BuildScripts = false,
                .CopyRuntime = false,
            };
        }

        Testing::TempDir Directory;
        Ref<Project> PreviousProject;
        std::filesystem::path ProjectDirectory;
        Ref<AssetManager> AssetManagerInstance;
        Ref<Project> ProjectInstance;
    };
}

TEST(ProjectExport, ProjectExporter_ExportsAssetsAndPackedScenes)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.AddScene(501, "other.epscene");
    fixture.AddTexture(600);
    std::filesystem::create_directories(fixture.ProjectDirectory / "Assets" / "Meshes");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
    EP_REQUIRE(result.Success);
    EXPECT_EQ((fixture.Options().ParentDirectory / "ExportGame").lexically_normal().string(), result.OutputPath.string());
    const auto debugDirectory = result.OutputPath / "Debug";
    EXPECT_TRUE(FS::Exists(debugDirectory / "Assets" / "texture.png"));
    EXPECT_TRUE(!FS::Exists(debugDirectory / "Assets" / "AssetRegistry.json"));
    EXPECT_TRUE(!FS::Exists(debugDirectory / "Assets" / "Scenes"));
    EXPECT_TRUE(!FS::Exists(debugDirectory / "Assets" / "Meshes"));
    EXPECT_EQ(2u, static_cast<uint32_t>(result.Warnings.size()));

    GameData gameData;
    EP_REQUIRE(gameData.Deserialize(debugDirectory / GameData::Filename));
    EXPECT_EQ(3u, static_cast<uint32_t>(gameData.AssetRegistry.size()));
    EXPECT_EQ(2u, static_cast<uint32_t>(gameData.PackedAssets.size()));

    const Ref<AssetManager> packedManager =
        CreateRef<AssetManager>(std::move(gameData.AssetRegistry), std::move(gameData.PackedAssets));
    const Ref<Scene> scene = packedManager->GetOrLoadAsset<Scene>(AssetHandle(500));
    EP_REQUIRE(scene != nullptr);
    EXPECT_TRUE(scene->GetPrimaryCameraEntity());
}


TEST(ProjectExport, ProjectExporter_PacksEngineShadersInsteadOfShippingThemLoose)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
    EP_REQUIRE(result.Success);

    GameData gameData;
    EP_REQUIRE(gameData.Deserialize(result.OutputPath / "Debug" / GameData::Filename));
    for (const auto* name : { "composite", "geometry", "imgui", "shadowDepth", "skybox", "tonemap", "wireframe" })
    {
        EP_REQUIRE(gameData.PackedShaders.contains(name));
        EXPECT_TRUE(!gameData.PackedShaders.at(name).empty());
    }

    // Every #include the engine shaders name must travel with them.
    EP_REQUIRE(!gameData.PackedShaderIncludes.empty());
    EXPECT_TRUE(gameData.PackedShaderIncludes.contains("Includes/platform.hlsli"));
    EXPECT_TRUE(gameData.PackedShaderIncludes.contains("Includes/lighting.hlsli"));
    for (const auto& source : gameData.PackedShaderIncludes | std::views::values)
        EXPECT_TRUE(!source.empty());

    for (const auto& entry : std::filesystem::recursive_directory_iterator(result.OutputPath))
    {
        const auto extension = entry.path().extension();
        EXPECT_TRUE(extension != ".hlsl");
        EXPECT_TRUE(extension != ".hlsli");
    }
}

TEST(ProjectExport, ProjectExporter_RejectsNonEmptyTargetWithoutMutation)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
    const auto target = fixture.Options().ParentDirectory / "ExportGame";
    std::filesystem::create_directories(target);
    const auto marker = target / "keep.txt";
    EXPECT_TRUE(FS::WriteText(marker, "keep", true));

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
    EXPECT_TRUE(!result.Success);
    EP_REQUIRE_EQ(1u, static_cast<uint32_t>(result.Errors.size()));
    EXPECT_EQ(std::string("Export target directory is not empty."), result.Errors.front());
    EXPECT_EQ(std::string("keep"), FS::ReadText(marker));
}

TEST(ProjectExport, ProjectExporter_AcceptsExistingEmptyTarget)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
    std::filesystem::create_directories(fixture.Options().ParentDirectory / "ExportGame");

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
    EXPECT_TRUE(result.Success);
    EXPECT_TRUE(FS::Exists(result.OutputPath / "Debug" / GameData::Filename));
}

TEST(ProjectExport, ProjectExporter_RequiresAnExportConfiguration)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    ProjectExportOptions options = fixture.Options();
    options.ExportDebug = false;

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EXPECT_TRUE(!result.Success);
    EXPECT_EQ(std::string("At least one export configuration must be selected."), result.Errors.front());
    EXPECT_TRUE(!FS::Exists(options.ParentDirectory / "ExportGame"));
}

TEST(ProjectExport, ProjectExporter_RequiresSourceDirectoryWhenBuildingRuntime)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
    ProjectExportOptions options = fixture.Options();
    options.BuildRuntime = true;
    options.CopyRuntime = true;

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EXPECT_TRUE(!result.Success);
    EXPECT_EQ(std::string("The engine source directory is required when building the runtime."), result.Errors.front());
    EXPECT_TRUE(!FS::Exists(options.ParentDirectory / "ExportGame"));
}

TEST(ProjectExport, ProjectExporter_ReportsMonotonicProgress)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
    ProjectExportOptions options = fixture.Options();
    std::vector<float> values;
    std::vector<std::string> phases;
    options.ProgressCallback = [&](const float value, const std::string_view phase)
    {
        values.emplace_back(value);
        phases.emplace_back(phase);
    };

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EP_REQUIRE(result.Success);
    EP_REQUIRE(!values.empty());
    EXPECT_NEAR(1.0f, values.back(), 0.0001f);
    EXPECT_TRUE(!phases.back().empty());
    for (size_t index = 1; index < values.size(); ++index)
        EXPECT_TRUE(values[index] >= values[index - 1]);
}

TEST(ProjectExport, ProjectExporter_ValidatesStartSceneBeforeCreatingTarget)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    const auto options = fixture.Options();

    ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EXPECT_TRUE(!result.Success);
    EXPECT_EQ(std::string("A start scene is not configured."), result.Errors.front());
    EXPECT_TRUE(!FS::Exists(options.ParentDirectory / "ExportGame"));

    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(999);
    result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EXPECT_TRUE(!result.Success);
    EXPECT_EQ(std::string("The configured start scene is not registered."), result.Errors.front());

    fixture.AddTexture(600);
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(600);
    result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EXPECT_TRUE(!result.Success);
    EXPECT_EQ(std::string("The configured start scene is not a scene asset."), result.Errors.front());
}

TEST(ProjectExport, ProjectExporter_CopiesRelocatableRuntimeDeployment)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
    const auto debugRuntimeDirectory = fixture.Directory.File("Runtime/Debug");
    const auto releaseRuntimeDirectory = fixture.Directory.File("Runtime/Release");
    const auto debugManagedDirectory = fixture.Directory.File("Managed/Debug");
    const auto releaseManagedDirectory = fixture.Directory.File("Managed/Release");
    for (const auto& runtimeDirectory : { debugRuntimeDirectory, releaseRuntimeDirectory })
    {
        std::filesystem::create_directories(runtimeDirectory / "Resources" / "Shaders");
        std::filesystem::create_directories(runtimeDirectory / "Resources" / "Fonts");
#if defined(EP_PLATFORM_WINDOWS)
        EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoRuntime.exe", "runtime", true));
        EXPECT_TRUE(FS::WriteText(runtimeDirectory / "dxcompiler.dll", "dxcompiler", true));
#else
        EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoRuntime", "runtime", true));
#endif
        EXPECT_TRUE(FS::WriteText(runtimeDirectory / "runtimeconfig.json", "runtime", true));
        EXPECT_TRUE(FS::WriteText(runtimeDirectory / "Resources" / "Shaders" / "composite.hlsl", "shader", true));
    }
#if defined(EP_PLATFORM_WINDOWS)
    EXPECT_TRUE(FS::WriteText(debugRuntimeDirectory / "EppoRuntime.pdb", "symbols", true));
#endif
    for (const auto& managedDirectory : { debugManagedDirectory, releaseManagedDirectory })
    {
        std::filesystem::create_directories(managedDirectory);
        EXPECT_TRUE(FS::WriteText(managedDirectory / "EppoScriptCore.dll", "managed", true));
        EXPECT_TRUE(FS::WriteText(managedDirectory / "EppoScriptCore.deps.json", "{}", true));
    }
    EXPECT_TRUE(FS::WriteText(debugManagedDirectory / "EppoScriptCore.pdb", "symbols", true));

    ProjectExportOptions options = fixture.Options();
    options.ExportRelease = true;
    options.BuildScripts = true;
    options.CopyRuntime = true;
    options.DebugRuntimeDirectory = debugRuntimeDirectory;
    options.ReleaseRuntimeDirectory = releaseRuntimeDirectory;
    options.DebugManagedDirectory = debugManagedDirectory;
    options.ReleaseManagedDirectory = releaseManagedDirectory;

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EP_REQUIRE(result.Success);
    EXPECT_TRUE(result.Warnings.empty());
    for (const auto* configuration : { "Debug", "Release" })
    {
        const auto outputDirectory = result.OutputPath / configuration;
#if defined(EP_PLATFORM_WINDOWS)
        EXPECT_TRUE(FS::Exists(outputDirectory / "ExportGame.exe"));
        EXPECT_TRUE(FS::Exists(outputDirectory / "dxcompiler.dll"));
#else
        EXPECT_TRUE(FS::Exists(outputDirectory / "ExportGame"));
#endif
        EXPECT_TRUE(FS::Exists(outputDirectory / "EppoScriptCore.dll"));
        EXPECT_TRUE(FS::Exists(outputDirectory / "EppoScriptCore.deps.json"));
        EXPECT_TRUE(FS::Exists(outputDirectory / "runtimeconfig.json"));
        // Engine resources are carried by Game.eppak; the runtime never reads them from disk.
        EXPECT_TRUE(!FS::Exists(outputDirectory / "Resources"));
        EXPECT_TRUE(FS::ReadText(outputDirectory / "EppoScriptCore.deps.json").find(fixture.ProjectDirectory.string()) == std::string::npos);
    }
#if defined(EP_PLATFORM_WINDOWS)
    EXPECT_TRUE(FS::Exists(result.OutputPath / "Debug" / "EppoRuntime.pdb"));
#endif
    EXPECT_TRUE(!FS::Exists(result.OutputPath / "Release" / "EppoRuntime.pdb"));
}

TEST(ProjectExport, ProjectExporter_PackagesTheManagedCoreUsedToBuildScripts)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    ExportProjectFixture fixture;
    fixture.AddScene(500, "start.epscene");
    fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);

    const auto runtimeDirectory = fixture.Directory.File("Runtime");
    std::filesystem::create_directories(runtimeDirectory / "Resources");
#if defined(EP_PLATFORM_WINDOWS)
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoRuntime.exe", "runtime", true));
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoRuntime.pdb", "symbols", true));
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "dxcompiler.dll", "dxcompiler", true));
#else
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoRuntime", "runtime", true));
#endif
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoScriptCore.dll", "stale", true));
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "EppoScriptCore.deps.json", "stale", true));
    EXPECT_TRUE(FS::WriteText(runtimeDirectory / "runtimeconfig.json", "runtime", true));

    ProjectExportOptions options = fixture.Options();
    options.CopyRuntime = true;
    options.DebugRuntimeDirectory = runtimeDirectory;
    options.DebugManagedDirectory = FS::GetExecutableDirectory();

    const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
    EP_REQUIRE(result.Success);
    EXPECT_TRUE(
        FS::ReadBytes(result.OutputPath / "Debug" / "EppoScriptCore.dll") ==
        FS::ReadBytes(FS::GetExecutableDirectory() / "EppoScriptCore.dll")
    );
    EXPECT_TRUE(
        FS::ReadBytes(result.OutputPath / "Debug" / "EppoScriptCore.deps.json") ==
        FS::ReadBytes(FS::GetExecutableDirectory() / "EppoScriptCore.deps.json")
    );
}
