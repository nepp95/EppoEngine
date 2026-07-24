#include "Support/EppoTest.h"
#include "Support/AppHarness.h"
#include "Support/TempDir.h"

#include "Core/Buffer.h"
#include "Core/BufferReader.h"
#include "Project/GameData.h"
#include "Project/Project.h"
#include "Project/ProjectExporter.h"
#include "Scene/Entity.h"
#include "Scene/SceneSerializer.h"

using namespace Eppo;

// Export reads the engine shaders from the live renderer, so these boot the graphical harness.
SUITE(ProjectExport)
{
    namespace
    {
        class ExportProjectFixture
        {
        public:
            explicit ExportProjectFixture(std::string name = "ExportGame")
                : PreviousProject(Project::GetActive()), ProjectDirectory(Directory.File("Project")), AssetManagerInstance(CreateRef<AssetManager>())
            {
                std::filesystem::create_directories(ProjectDirectory / "Assets" / "Scenes");
                ProjectInstance = Project::New(ProjectSpecification{
                    .Name = std::move(name),
                    .ProjectDirectory = ProjectDirectory,
                }, AssetManagerInstance);
            }

            ~ExportProjectFixture()
            {
                Project::SetActive(PreviousProject);
            }

            auto AddScene(const uint64_t handle, const std::string& filename, const bool primaryCamera = true) -> Ref<Scene>
            {
                const Ref<Scene> scene = CreateRef<Scene>();
                scene->Handle = AssetHandle(handle);
                Entity camera = scene->CreateEntity("Camera");
                if (primaryCamera)
                    camera.AddComponent<CameraComponent>();

                const auto relativePath = std::filesystem::path("Scenes") / filename;
                const auto path = ProjectDirectory / "Assets" / relativePath;
                CHECK(SceneSerializer(scene).Serialize(path));
                CHECK(AssetManagerInstance->CreateAsset(path, scene));
                return scene;
            }

            auto AddTexture(const uint64_t handle, const std::string& filename = "texture.png") -> std::filesystem::path
            {
                const auto path = ProjectDirectory / "Assets" / filename;
                const std::vector<char> bytes{ 't', 'e', 'x' };
                CHECK(FS::WriteBytes(path, bytes, true));
                const Ref<Asset> asset = CreateRef<Asset>();
                asset->Handle = AssetHandle(handle);
                CHECK(AssetManagerInstance->CreateAsset(path, asset));
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

    TEST(ProjectExporter_ExportsAssetsAndPackedScenes)
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
        REQUIRE CHECK(result.Success);
        CHECK_EQUAL((fixture.Options().ParentDirectory / "ExportGame").lexically_normal().string(), result.OutputPath.string());
        const auto debugDirectory = result.OutputPath / "Debug";
        CHECK(FS::Exists(debugDirectory / "Assets" / "texture.png"));
        CHECK(!FS::Exists(debugDirectory / "Assets" / "AssetRegistry.json"));
        CHECK(!FS::Exists(debugDirectory / "Assets" / "Scenes"));
        CHECK(!FS::Exists(debugDirectory / "Assets" / "Meshes"));
        CHECK_EQUAL(2u, static_cast<uint32_t>(result.Warnings.size()));

        GameData gameData;
        REQUIRE CHECK(gameData.Deserialize(debugDirectory / GameData::Filename));
        CHECK_EQUAL(3u, static_cast<uint32_t>(gameData.AssetRegistry.size()));
        CHECK_EQUAL(2u, static_cast<uint32_t>(gameData.PackedAssets.size()));

        const Ref<AssetManager> packedManager = CreateRef<AssetManager>(std::move(gameData.AssetRegistry), std::move(gameData.PackedAssets));
        const Ref<Scene> scene = packedManager->GetOrLoadAsset<Scene>(AssetHandle(500));
        REQUIRE CHECK(scene != nullptr);
        CHECK(scene->GetPrimaryCameraEntity());
    }


    TEST(ProjectExporter_RejectsNonEmptyTargetWithoutMutation)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        fixture.AddScene(500, "start.epscene");
        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
        const auto target = fixture.Options().ParentDirectory / "ExportGame";
        std::filesystem::create_directories(target);
        const auto marker = target / "keep.txt";
        CHECK(FS::WriteText(marker, "keep", true));

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
        CHECK(!result.Success);
        REQUIRE CHECK_EQUAL(1u, static_cast<uint32_t>(result.Errors.size()));
        CHECK_EQUAL(std::string("Export target directory is not empty."), result.Errors.front());
        CHECK_EQUAL(std::string("keep"), FS::ReadText(marker));
    }

    TEST(ProjectExporter_AcceptsExistingEmptyTarget)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        fixture.AddScene(500, "start.epscene");
        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);
        std::filesystem::create_directories(fixture.Options().ParentDirectory / "ExportGame");

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
        CHECK(result.Success);
        CHECK(FS::Exists(result.OutputPath / "Debug" / GameData::Filename));
    }

    TEST(ProjectExporter_RequiresAnExportConfiguration)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        ProjectExportOptions options = fixture.Options();
        options.ExportDebug = false;

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
        CHECK(!result.Success);
        CHECK_EQUAL(std::string("At least one export configuration must be selected."), result.Errors.front());
        CHECK(!FS::Exists(options.ParentDirectory / "ExportGame"));
    }

    TEST(ProjectExporter_ReportsMonotonicProgress)
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
        REQUIRE CHECK(result.Success);
        REQUIRE CHECK(!values.empty());
        CHECK_CLOSE(1.0f, values.back(), 0.0001f);
        CHECK(!phases.back().empty());
        for (size_t index = 1; index < values.size(); ++index)
            CHECK(values[index] >= values[index - 1]);
    }

    TEST(ProjectExporter_IsolatesScriptFieldsAndRelationshipNotices)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        const Ref<Scene> scene = CreateRef<Scene>();
        scene->Handle = AssetHandle(500);
        Entity camera = scene->CreateEntityWithUUID(UUID(100), "Camera");
        camera.AddComponent<CameraComponent>();
        camera.AddComponent<ScriptComponent>(std::string("Game.Player"));
        camera.AddComponent<RelationshipComponent>().Parent = UUID(999);

        ScriptFieldStorage sourceFields;
        ScriptFieldValue speed;
        speed.Type = ScriptFieldType::Float;
        speed.Set(4.5f);
        sourceFields[camera.GetUUID()]["Speed"] = speed;

        const auto scenePath = fixture.ProjectDirectory / "Assets" / "Scenes" / "start.epscene";
        CHECK(SceneSerializer(scene, { .ScriptFields = &sourceFields }).Serialize(scenePath));
        CHECK(fixture.AssetManagerInstance->CreateAsset(scenePath, scene));
        fixture.ProjectInstance->GetSpecification().StartScene = scene->Handle;
        SceneSerializer::ConsumeRelationshipRepairNotices();

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
        REQUIRE CHECK(result.Success);
        CHECK(SceneSerializer::ConsumeRelationshipRepairNotices().empty());

        GameData gameData;
        REQUIRE CHECK(gameData.Deserialize(result.OutputPath / "Debug" / GameData::Filename));
        const auto packedAsset = gameData.PackedAssets.find(scene->Handle);
        REQUIRE CHECK(packedAsset != gameData.PackedAssets.end());
        Buffer payload(packedAsset->second.Payload.data(), packedAsset->second.Payload.size());
        BufferReader reader(payload);
        const Ref<Scene> loaded = CreateRef<Scene>();
        loaded->Handle = scene->Handle;
        ScriptFieldStorage loadedFields;
        REQUIRE CHECK(SceneSerializer(loaded, { .ScriptFields = &loadedFields }).Deserialize(reader));
        REQUIRE CHECK(loadedFields.contains(camera.GetUUID()));
        CHECK_CLOSE(4.5f, loadedFields.at(camera.GetUUID()).at("Speed").Get<float>(), 0.0001f);
    }

    TEST(ProjectExporter_ValidatesStartSceneBeforeCreatingTarget)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        const auto options = fixture.Options();

        ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
        CHECK(!result.Success);
        CHECK_EQUAL(std::string("A start scene is not configured."), result.Errors.front());
        CHECK(!FS::Exists(options.ParentDirectory / "ExportGame"));

        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(999);
        result = ProjectExporter(fixture.ProjectInstance).Export(options);
        CHECK(!result.Success);
        CHECK_EQUAL(std::string("The configured start scene is not registered."), result.Errors.front());

        fixture.AddTexture(600);
        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(600);
        result = ProjectExporter(fixture.ProjectInstance).Export(options);
        CHECK(!result.Success);
        CHECK_EQUAL(std::string("The configured start scene is not a scene asset."), result.Errors.front());
    }

    TEST(ProjectExporter_RejectsUnloadableOrCameraLessStartScene)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        fixture.AddScene(500, "start.epscene", false);
        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);

        ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
        CHECK(!result.Success);
        CHECK_EQUAL(std::string("The start scene does not contain a primary camera."), result.Errors.front());

        std::filesystem::remove(fixture.ProjectDirectory / "Assets" / "Scenes" / "start.epscene");
        result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
        CHECK(!result.Success);
        CHECK_EQUAL(std::string("Scene 'Scenes/start.epscene' could not be loaded."), result.Errors.front());
    }

    TEST(ProjectExporter_SanitizesTargetNameAndSkipsExternalSteps)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture("Bad/Game");
        fixture.AddScene(500, "start.epscene");
        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(fixture.Options());
        REQUIRE CHECK(result.Success);
        CHECK_EQUAL(std::string("Bad_Game"), result.OutputPath.filename().string());
        CHECK_EQUAL(std::string("Project name was sanitized to 'Bad_Game'."), result.Warnings.front());
    }

    TEST(ProjectExporter_CopiesRelocatableRuntimeDeployment)
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
            CHECK(FS::WriteText(runtimeDirectory / "EppoRuntime.exe", "runtime", true));
            CHECK(FS::WriteText(runtimeDirectory / "dxcompiler.dll", "dxcompiler", true));
            #else
            CHECK(FS::WriteText(runtimeDirectory / "EppoRuntime", "runtime", true));
            #endif
            CHECK(FS::WriteText(runtimeDirectory / "runtimeconfig.json", "runtime", true));
            CHECK(FS::WriteText(runtimeDirectory / "Resources" / "Shaders" / "composite.vert", "shader", true));
        }
        #if defined(EP_PLATFORM_WINDOWS)
        CHECK(FS::WriteText(debugRuntimeDirectory / "EppoRuntime.pdb", "symbols", true));
        #endif
        for (const auto& managedDirectory : { debugManagedDirectory, releaseManagedDirectory })
        {
            std::filesystem::create_directories(managedDirectory);
            CHECK(FS::WriteText(managedDirectory / "EppoScriptCore.dll", "managed", true));
            CHECK(FS::WriteText(managedDirectory / "EppoScriptCore.deps.json", "{}", true));
        }
        CHECK(FS::WriteText(debugManagedDirectory / "EppoScriptCore.pdb", "symbols", true));

        ProjectExportOptions options = fixture.Options();
        options.ExportRelease = true;
        options.BuildScripts = true;
        options.CopyRuntime = true;
        options.DebugRuntimeDirectory = debugRuntimeDirectory;
        options.ReleaseRuntimeDirectory = releaseRuntimeDirectory;
        options.DebugManagedDirectory = debugManagedDirectory;
        options.ReleaseManagedDirectory = releaseManagedDirectory;

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
        REQUIRE CHECK(result.Success);
        CHECK(result.Warnings.empty());
        for (const auto* configuration : { "Debug", "Release" })
        {
            const auto outputDirectory = result.OutputPath / configuration;
            #if defined(EP_PLATFORM_WINDOWS)
            CHECK(FS::Exists(outputDirectory / "ExportGame.exe"));
            CHECK(FS::Exists(outputDirectory / "dxcompiler.dll"));
            #else
            CHECK(FS::Exists(outputDirectory / "ExportGame"));
            #endif
            CHECK(FS::Exists(outputDirectory / "EppoScriptCore.dll"));
            CHECK(FS::Exists(outputDirectory / "EppoScriptCore.deps.json"));
            CHECK(FS::Exists(outputDirectory / "runtimeconfig.json"));
            CHECK(FS::Exists(outputDirectory / "Resources" / "Shaders" / "composite.vert"));
            CHECK(!FS::Exists(outputDirectory / "Resources" / "Fonts"));
            CHECK(FS::ReadText(outputDirectory / "EppoScriptCore.deps.json").find(fixture.ProjectDirectory.string()) == std::string::npos);
        }
        #if defined(EP_PLATFORM_WINDOWS)
        CHECK(FS::Exists(result.OutputPath / "Debug" / "EppoRuntime.pdb"));
        #endif
        CHECK(!FS::Exists(result.OutputPath / "Release" / "EppoRuntime.pdb"));
    }

    TEST(ProjectExporter_PackagesTheManagedCoreUsedToBuildScripts)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        ExportProjectFixture fixture;
        fixture.AddScene(500, "start.epscene");
        fixture.ProjectInstance->GetSpecification().StartScene = AssetHandle(500);

        const auto runtimeDirectory = fixture.Directory.File("Runtime");
        std::filesystem::create_directories(runtimeDirectory / "Resources");
        #if defined(EP_PLATFORM_WINDOWS)
        CHECK(FS::WriteText(runtimeDirectory / "EppoRuntime.exe", "runtime", true));
        CHECK(FS::WriteText(runtimeDirectory / "EppoRuntime.pdb", "symbols", true));
        CHECK(FS::WriteText(runtimeDirectory / "dxcompiler.dll", "dxcompiler", true));
        #else
        CHECK(FS::WriteText(runtimeDirectory / "EppoRuntime", "runtime", true));
        #endif
        CHECK(FS::WriteText(runtimeDirectory / "EppoScriptCore.dll", "stale", true));
        CHECK(FS::WriteText(runtimeDirectory / "EppoScriptCore.deps.json", "stale", true));
        CHECK(FS::WriteText(runtimeDirectory / "runtimeconfig.json", "runtime", true));

        ProjectExportOptions options = fixture.Options();
        options.CopyRuntime = true;
        options.DebugRuntimeDirectory = runtimeDirectory;
        options.DebugManagedDirectory = FS::GetRootDirectory();

        const ProjectExportResult result = ProjectExporter(fixture.ProjectInstance).Export(options);
        REQUIRE CHECK(result.Success);
        CHECK(FS::ReadBytes(result.OutputPath / "Debug" / "EppoScriptCore.dll") == FS::ReadBytes(FS::GetRootDirectory() / "EppoScriptCore.dll"));
        CHECK(FS::ReadBytes(result.OutputPath / "Debug" / "EppoScriptCore.deps.json") == FS::ReadBytes(FS::GetRootDirectory() / "EppoScriptCore.deps.json"));
    }
}
