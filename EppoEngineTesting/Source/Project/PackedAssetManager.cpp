#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Asset/AssetManager.h"
#include "Project/Project.h"
#include "Scene/Entity.h"
#include "Scene/SceneSerializer.h"

using namespace Eppo;

TEST(Project, AssetManager_PackedScene_LoadsLazilyFromOwnedPayload)
{
    const Ref<Project> previous = Project::GetActive();
    const Testing::TempDir dir;
    const auto projectDirectory = dir.File("Project");
    std::filesystem::create_directories(projectDirectory / "Assets" / "Scenes");

    // A packed scene is the .epscene file's bytes; author one, capture its bytes, then remove
    // the file so the lazy load has to materialize it back from the owned payload.
    const auto scenePath = projectDirectory / "Assets" / "Scenes" / "packed.epscene";
    const Ref<Scene> authored = CreateRef<Scene>();
    authored->Handle = AssetHandle(500);
    authored->CreateEntityWithUUID(Eppo::UUID(501), "PackedEntity");
    EP_REQUIRE(SceneSerializer(authored).Serialize(scenePath));
    const auto bytes = FS::ReadBytes(scenePath);
    const Buffer payload = Buffer::Copy(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    std::filesystem::remove(scenePath);

    std::map<AssetHandle, AssetMetadata> registry;
    registry.emplace(500, AssetMetadata{ AssetHandle(500), AssetType::Scene, "Scenes/packed.epscene" });
    std::map<AssetHandle, PackedAssetData> packedAssets;
    packedAssets.emplace(500, PackedAssetData{ AssetType::Scene, payload });

    const Ref<AssetManager> manager = CreateRef<AssetManager>(std::move(registry), std::move(packedAssets));
    Project::New(ProjectSpecification{ .Name = "Packed", .ProjectDirectory = projectDirectory }, manager);

    EP_REQUIRE(manager->HasAssetData(AssetHandle(500)));
    EXPECT_TRUE(!manager->IsAssetLoaded(AssetHandle(500)));

    const Ref<Scene> scene = manager->GetOrLoadAsset<Scene>(AssetHandle(500));
    EP_REQUIRE(scene != nullptr);
    EXPECT_TRUE(manager->IsAssetLoaded(AssetHandle(500)));
    EXPECT_EQ(500, static_cast<uint64_t>(scene->Handle));
    const Entity entity = scene->GetEntityByUUID(Eppo::UUID(501));
    EP_REQUIRE(entity);
    EXPECT_EQ(std::string("PackedEntity"), entity.GetName());

    Project::SetActive(previous);
}

TEST(Project, AssetManager_InvalidPackedScene_ReturnsNullWithoutCaching)
{
    const Ref<Project> previous = Project::GetActive();
    const Testing::TempDir dir;
    const auto projectDirectory = dir.File("Project");
    std::filesystem::create_directories(projectDirectory / "Assets" / "Scenes");

    const std::array<uint8_t, 3> garbage{ 1, 2, 3 };
    std::map<AssetHandle, AssetMetadata> registry;
    registry.emplace(500, AssetMetadata{ AssetHandle(500), AssetType::Scene, "Scenes/packed.epscene" });
    std::map<AssetHandle, PackedAssetData> packedAssets;
    packedAssets.emplace(500, PackedAssetData{ AssetType::Scene, Buffer::Copy(garbage.data(), garbage.size()) });

    const Ref<AssetManager> manager = CreateRef<AssetManager>(std::move(registry), std::move(packedAssets));
    Project::New(ProjectSpecification{ .Name = "Packed", .ProjectDirectory = projectDirectory }, manager);

    EXPECT_TRUE(manager->GetOrLoadAsset(AssetHandle(500)) == nullptr);
    EXPECT_TRUE(!manager->IsAssetLoaded(AssetHandle(500)));

    Project::SetActive(previous);
}

TEST(Project, Project_RuntimeConstruction_UsesProvidedAssetManager)
{
    const Ref<Project> previous = Project::GetActive();
    const Ref<AssetManager> assetManager = CreateRef<AssetManager>();
    const ProjectSpecification specification{
        .Name = "RuntimeGame",
        .ProjectDirectory = "C:/Export/RuntimeGame",
        .StartScene = AssetHandle(500),
    };

    const Ref<Project> project = Project::New(specification, assetManager);
    EP_REQUIRE(project != nullptr);
    EXPECT_TRUE(Project::GetActive() == project);
    EXPECT_TRUE(project->GetAssetManager() == assetManager);
    EXPECT_EQ(std::string("RuntimeGame"), project->GetSpecification().Name);
    EXPECT_EQ(500, static_cast<uint64_t>(project->GetSpecification().StartScene));

    Project::SetActive(previous);
}
