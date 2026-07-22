#include "Support/EppoTest.h"

#include "Asset/AssetImporter.h"
#include "Asset/AssetManager.h"
#include "Core/BufferReader.h"
#include "Core/BufferWriter.h"
#include "Project/GameData.h"
#include "Project/Project.h"
#include "Scene/Entity.h"
#include "Scene/SceneSerializer.h"

using namespace Eppo;

SUITE(Project)
{
    namespace
    {
        auto MakePackedScene(const uint64_t handle) -> PackedAssetData
        {
            const Ref<Scene> scene = CreateRef<Scene>();
            scene->Handle = AssetHandle(handle);
            scene->CreateEntityWithUUID(Eppo::UUID(501), "PackedEntity");

            BufferWriter sizingWriter;
            if (!SceneSerializer(scene).Serialize(sizingWriter))
                return {};
            Buffer buffer(sizingWriter.GetSize());
            BufferWriter writer(buffer);
            if (!SceneSerializer(scene).Serialize(writer))
            {
                buffer.Release();
                return {};
            }

            PackedAssetData result{ AssetType::Scene, { buffer.Data, buffer.Data + buffer.Size } };
            buffer.Release();
            return result;
        }
    }

    TEST(AssetManager_PackedScene_LoadsLazilyFromOwnedPayload)
    {
        std::map<AssetHandle, AssetMetadata> registry;
        registry.emplace(500, AssetMetadata{ AssetHandle(500), AssetType::Scene, "Scenes/packed.epscene" });
        std::map<AssetHandle, PackedAssetData> packedAssets;
        packedAssets.emplace(500, MakePackedScene(500));

        AssetManager manager(std::move(registry), std::move(packedAssets));
        REQUIRE CHECK(manager.HasAssetData(AssetHandle(500)));
        CHECK(!manager.IsAssetLoaded(AssetHandle(500)));

        const Ref<Scene> scene = manager.GetOrLoadAsset<Scene>(AssetHandle(500));
        REQUIRE CHECK(scene != nullptr);
        CHECK(manager.IsAssetLoaded(AssetHandle(500)));
        CHECK_EQUAL(500, static_cast<uint64_t>(scene->Handle));
        const Entity entity = scene->GetEntityByUUID(Eppo::UUID(501));
        REQUIRE CHECK(entity);
        CHECK_EQUAL(std::string("PackedEntity"), entity.GetName());
    }

    TEST(AssetManager_InvalidPackedScene_ReturnsNullWithoutCaching)
    {
        std::map<AssetHandle, AssetMetadata> registry;
        registry.emplace(500, AssetMetadata{ AssetHandle(500), AssetType::Scene, "Scenes/packed.epscene" });
        std::map<AssetHandle, PackedAssetData> packedAssets;
        packedAssets.emplace(500, PackedAssetData{ AssetType::Scene, { 1, 2, 3 } });
        AssetManager manager(std::move(registry), std::move(packedAssets));

        CHECK(manager.GetOrLoadAsset(AssetHandle(500)) == nullptr);
        CHECK(!manager.IsAssetLoaded(AssetHandle(500)));
    }

    TEST(AssetImporter_PackedUnsupportedType_ReturnsNull)
    {
        std::array<uint8_t, 4> bytes{};
        Buffer buffer(bytes.data(), bytes.size());
        BufferReader reader(buffer);
        CHECK(AssetImporter::ImportAsset(AssetHandle(500), AssetType::Mesh, reader) == nullptr);
        CHECK_EQUAL(0, reader.GetOffset());
    }

    TEST(Project_RuntimeConstruction_UsesProvidedAssetManager)
    {
        const Ref<Project> previous = Project::GetActive();
        const Ref<AssetManager> assetManager = CreateRef<AssetManager>();
        const ProjectSpecification specification{
            .Name = "RuntimeGame",
            .ProjectDirectory = "C:/Export/RuntimeGame",
            .StartScene = AssetHandle(500),
        };

        const Ref<Project> project = Project::New(specification, assetManager);
        REQUIRE CHECK(project != nullptr);
        CHECK(Project::GetActive() == project);
        CHECK(project->GetAssetManager() == assetManager);
        CHECK_EQUAL(std::string("RuntimeGame"), project->GetSpecification().Name);
        CHECK_EQUAL(500, static_cast<uint64_t>(project->GetSpecification().StartScene));

        Project::SetActive(previous);
    }
}
