#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Asset/PackFormat.h"
#include "Core/BufferWriter.h"
#include "Project/GameData.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

using namespace Eppo;

SUITE(Project)
{
    namespace
    {
        struct RawRegistryEntry
        {
            uint64_t Handle;
            uint8_t Type;
            std::string Path;
        };

        struct RawPackedAsset
        {
            uint64_t Handle;
            uint8_t Type;
            std::vector<uint8_t> Payload;
        };

        auto ScenePayload(const uint64_t handle) -> std::vector<uint8_t>
        {
            const Ref<Scene> scene = CreateRef<Scene>();
            scene->Handle = AssetHandle(handle);

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

            std::vector<uint8_t> payload(buffer.Data, buffer.Data + buffer.Size);
            buffer.Release();
            return payload;
        }

        auto WriteRawPack(const std::filesystem::path& path, const std::vector<RawRegistryEntry>& registry,
            const std::vector<RawPackedAsset>& packedAssets, const uint32_t magic = PackFormat::Package.Magic,
            const uint32_t version = PackFormat::Package.Version, const uint64_t startScene = 100, const bool trailingByte = false) -> bool
        {
            const auto write = [&](BufferWriter& writer)
            {
                if (!writer.Write(magic) || !writer.Write(version) || !writer.WriteString("Game") || !writer.Write(startScene)
                    || !writer.Write(static_cast<uint32_t>(registry.size())))
                    return false;

                for (const auto& entry : registry)
                {
                    if (!writer.Write(entry.Handle) || !writer.Write(entry.Type) || !writer.WriteString(entry.Path))
                        return false;
                }

                if (!writer.Write(static_cast<uint32_t>(packedAssets.size())))
                    return false;
                for (const auto& asset : packedAssets)
                {
                    if (!writer.Write(asset.Handle) || !writer.Write(asset.Type) || !writer.Write(static_cast<uint64_t>(asset.Payload.size()))
                        || !writer.WriteBytes(asset.Payload.data(), asset.Payload.size()))
                        return false;
                }

                return !trailingByte || writer.Write(uint8_t{ 0xff });
            };

            BufferWriter sizingWriter;
            if (!write(sizingWriter))
                return false;
            Buffer buffer(sizingWriter.GetSize());
            BufferWriter writer(buffer);
            const bool result = write(writer) && FS::WriteBytes(path, buffer.Data, buffer.Size, true);
            buffer.Release();
            return result;
        }
    }

    TEST(GameData_FilenameIsFixed)
    {
        CHECK_EQUAL(std::string("Game.eppak"), std::string(GameData::Filename));
    }

    TEST(GameData_DeterministicRoundTripPreservesDeploymentData)
    {
        const Testing::TempDir dir;
        const auto firstPath = dir.File("first.eppak");
        const auto secondPath = dir.File("second.eppak");

        GameData first;
        first.ProjectName = "TestGame";
        first.StartScene = AssetHandle(100);
        first.AssetRegistry.emplace(200, AssetMetadata{ AssetHandle(200), AssetType::Texture, "Textures\\image.png" });
        first.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes\\start.epscene" });
        first.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, ScenePayload(100) });

        GameData second;
        second.ProjectName = first.ProjectName;
        second.StartScene = first.StartScene;
        second.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
        second.AssetRegistry.emplace(200, AssetMetadata{ AssetHandle(200), AssetType::Texture, "Textures/image.png" });
        second.PackedAssets.emplace(100, first.PackedAssets.at(100));

		REQUIRE CHECK(first.Serialize(firstPath));
		REQUIRE CHECK(second.Serialize(secondPath));
		const auto firstBytes = FS::ReadBytes(firstPath);
		const auto secondBytes = FS::ReadBytes(secondPath);
		CHECK_EQUAL(firstBytes.size(), secondBytes.size());
		CHECK_ARRAY_EQUAL(firstBytes.data(), secondBytes.data(), firstBytes.size());

        GameData loaded;
        REQUIRE CHECK(loaded.Deserialize(firstPath));
        CHECK_EQUAL(std::string("TestGame"), loaded.ProjectName);
        CHECK_EQUAL(100, static_cast<uint64_t>(loaded.StartScene));
        CHECK_EQUAL(2, loaded.AssetRegistry.size());
        CHECK_EQUAL(std::string("Scenes/start.epscene"), loaded.AssetRegistry.at(100).Filepath.generic_string());
        CHECK_EQUAL(std::string("Textures/image.png"), loaded.AssetRegistry.at(200).Filepath.generic_string());
        CHECK_EQUAL(1, loaded.PackedAssets.size());
        CHECK(loaded.PackedAssets.at(100).Type == AssetType::Scene);
        CHECK_ARRAY_EQUAL(first.PackedAssets.at(100).Payload.data(), loaded.PackedAssets.at(100).Payload.data(), first.PackedAssets.at(100).Payload.size());
    }

    TEST(GameData_ExcludesRuntimeGeneratedRegistryEntries)
    {
        const Testing::TempDir dir;
        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.AssetRegistry.emplace(1, AssetMetadata{ AssetHandle(1), AssetType::Mesh, {}, true });
        data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
        data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, ScenePayload(100) });
        REQUIRE CHECK(data.Serialize(dir.File("runtime-assets.eppak")));

        GameData loaded;
        REQUIRE CHECK(loaded.Deserialize(dir.File("runtime-assets.eppak")));
        CHECK_EQUAL(1, loaded.AssetRegistry.size());
        CHECK(loaded.AssetRegistry.contains(100));
        CHECK(!loaded.AssetRegistry.contains(1));
    }

    TEST(GameData_RejectsMissingWrongOrTruncatedHeaderAndTrailingBytes)
    {
        const Testing::TempDir dir;
        GameData loaded;
        CHECK(!loaded.Deserialize(dir.File("missing.eppak")));

        const auto payload = ScenePayload(100);
        REQUIRE CHECK(WriteRawPack(dir.File("wrong-magic.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 100, 2, payload } }, 0));
        CHECK(!loaded.Deserialize(dir.File("wrong-magic.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("wrong-version.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 100, 2, payload } }, PackFormat::Package.Magic, 99));
        CHECK(!loaded.Deserialize(dir.File("wrong-version.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("trailing.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 100, 2, payload } }, PackFormat::Package.Magic, PackFormat::Package.Version, 100, true));
        CHECK(!loaded.Deserialize(dir.File("trailing.eppak")));

        const auto validPath = dir.File("valid.eppak");
        REQUIRE CHECK(WriteRawPack(validPath, { { 100, 2, "Scenes/start.epscene" } }, { { 100, 2, payload } }));
        const auto validBytes = FS::ReadBytes(validPath);
        for (size_t size = 0; size < validBytes.size(); size++)
        {
            const auto truncatedPath = dir.File("truncated.eppak");
            REQUIRE CHECK(FS::WriteBytes(truncatedPath, validBytes.data(), size, true));
            CHECK(!loaded.Deserialize(truncatedPath));
        }
    }

    TEST(GameData_RejectsInvalidRegistryRecords)
    {
        const Testing::TempDir dir;
        const auto payload = ScenePayload(100);
        GameData loaded;

        REQUIRE CHECK(WriteRawPack(dir.File("duplicate.eppak"),
            { { 100, 2, "Scenes/start.epscene" }, { 100, 2, "Scenes/other.epscene" } }, { { 100, 2, payload } }));
        CHECK(!loaded.Deserialize(dir.File("duplicate.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("null.eppak"), { { 0, 2, "Scenes/start.epscene" } }, {}));
        CHECK(!loaded.Deserialize(dir.File("null.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("none-type.eppak"), { { 100, 0, "Scenes/start.epscene" } }, {}));
        CHECK(!loaded.Deserialize(dir.File("none-type.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("unknown-type.eppak"), { { 100, 5, "Scenes/start.epscene" } }, {}));
        CHECK(!loaded.Deserialize(dir.File("unknown-type.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("absolute.eppak"), { { 100, 2, "C:/Game/start.epscene" } }, { { 100, 2, payload } }));
        CHECK(!loaded.Deserialize(dir.File("absolute.eppak")));
    }

    TEST(GameData_RejectsInvalidPackedAssetRecords)
    {
        const Testing::TempDir dir;
        const auto scenePayload = ScenePayload(100);
        const auto wrongScenePayload = ScenePayload(101);
        GameData loaded;

        REQUIRE CHECK(WriteRawPack(dir.File("missing-scene.eppak"), { { 100, 2, "Scenes/start.epscene" } }, {}));
        CHECK(!loaded.Deserialize(dir.File("missing-scene.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("duplicate.eppak"), { { 100, 2, "Scenes/start.epscene" } },
            { { 100, 2, scenePayload }, { 100, 2, scenePayload } }));
        CHECK(!loaded.Deserialize(dir.File("duplicate.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("null.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 0, 2, scenePayload } }));
        CHECK(!loaded.Deserialize(dir.File("null.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("orphan.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 101, 2, wrongScenePayload } }));
        CHECK(!loaded.Deserialize(dir.File("orphan.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("type-mismatch.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 100, 1, scenePayload } }));
        CHECK(!loaded.Deserialize(dir.File("type-mismatch.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("handle-mismatch.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 100, 2, wrongScenePayload } }));
        CHECK(!loaded.Deserialize(dir.File("handle-mismatch.eppak")));
        REQUIRE CHECK(WriteRawPack(dir.File("unknown-type.eppak"), { { 100, 2, "Scenes/start.epscene" } }, { { 100, 5, scenePayload } }));
        CHECK(!loaded.Deserialize(dir.File("unknown-type.eppak")));
    }
}
