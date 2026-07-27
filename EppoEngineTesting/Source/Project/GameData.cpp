#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Asset/PackFormat.h"
#include "Project/GameData.h"

#include <algorithm>

using namespace Eppo;

SUITE(Project)
{
    namespace
    {
        // A packed asset's payload is carried opaquely, so a deterministic byte run
        // is enough to exercise the round-trip without depending on scene contents.
        auto MakePayload(const uint64_t seed, const uint64_t size = 64) -> Buffer
        {
            Buffer payload(size);
            for (uint64_t i = 0; i < size; i++)
                payload.Data[i] = static_cast<uint8_t>(seed + i);
            return payload;
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
        first.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });

        // Same content, inserted in a different order: the ordered maps must still serialize identically.
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
        CHECK_ARRAY_EQUAL(first.PackedAssets.at(100).Payload.Data, loaded.PackedAssets.at(100).Payload.Data, first.PackedAssets.at(100).Payload.Size);
    }

    TEST(GameData_ExcludesRuntimeGeneratedRegistryEntries)
    {
        const Testing::TempDir dir;
        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.AssetRegistry.emplace(1, AssetMetadata{ AssetHandle(1), AssetType::Mesh, {}, true });
        data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
        data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });
        REQUIRE CHECK(data.Serialize(dir.File("runtime-assets.eppak")));

        GameData loaded;
        REQUIRE CHECK(loaded.Deserialize(dir.File("runtime-assets.eppak")));
        CHECK_EQUAL(1, loaded.AssetRegistry.size());
        CHECK(loaded.AssetRegistry.contains(100));
        CHECK(!loaded.AssetRegistry.contains(1));
    }

    TEST(GameData_RoundTripPreservesPackedShaderSources)
    {
        const Testing::TempDir dir;
        const auto path = dir.File("shaders.eppak");

        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
        data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });
        data.PackedShaders.emplace("geometry", PackedShaderData{ {
            { nvrhi::ShaderType::Vertex, "vertex-source" },
            { nvrhi::ShaderType::Pixel, "pixel-source" },
        } });
        data.PackedShaders.emplace("composite", PackedShaderData{ {
            { nvrhi::ShaderType::Vertex, "composite-vertex" },
        } });
        REQUIRE CHECK(data.Serialize(path));

        GameData loaded;
        REQUIRE CHECK(loaded.Deserialize(path));
        CHECK_EQUAL(2, loaded.PackedShaders.size());
        CHECK_EQUAL(2, loaded.PackedShaders.at("geometry").ShaderSources.size());
        CHECK_EQUAL(std::string("vertex-source"), loaded.PackedShaders.at("geometry").ShaderSources.at(nvrhi::ShaderType::Vertex));
        CHECK_EQUAL(std::string("pixel-source"), loaded.PackedShaders.at("geometry").ShaderSources.at(nvrhi::ShaderType::Pixel));
        CHECK_EQUAL(1, loaded.PackedShaders.at("composite").ShaderSources.size());
        CHECK_EQUAL(std::string("composite-vertex"), loaded.PackedShaders.at("composite").ShaderSources.at(nvrhi::ShaderType::Vertex));
    }

    TEST(GameData_RoundTripAllowsNoPackedShaders)
    {
        const Testing::TempDir dir;
        const auto path = dir.File("no-shaders.eppak");

        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
        data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });
        REQUIRE CHECK(data.Serialize(path));

        GameData loaded;
        REQUIRE CHECK(loaded.Deserialize(path));
        CHECK(loaded.PackedShaders.empty());
        CHECK(loaded.PackedShaderIncludes.empty());
    }

    TEST(GameData_RoundTripPreservesPackedShaderIncludes)
    {
        const Testing::TempDir dir;
        const auto path = dir.File("includes.eppak");

        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.PackedShaders.emplace("geometry", PackedShaderData{ {
            { nvrhi::ShaderType::Vertex, "#include \"Includes/platform.hlsli\"\nvertex-source" },
        } });
        // Keyed by the path relative to Resources/Shaders, which is exactly what an #include names.
        data.PackedShaderIncludes.emplace("Includes/platform.hlsli", "platform-source");
        data.PackedShaderIncludes.emplace("Includes/lighting.hlsli", "lighting-source");
        REQUIRE CHECK(data.Serialize(path));

        GameData loaded;
        REQUIRE CHECK(loaded.Deserialize(path));
        CHECK_EQUAL(2, loaded.PackedShaderIncludes.size());
        CHECK_EQUAL(std::string("platform-source"), loaded.PackedShaderIncludes.at("Includes/platform.hlsli"));
        CHECK_EQUAL(std::string("lighting-source"), loaded.PackedShaderIncludes.at("Includes/lighting.hlsli"));
    }

    // The shader block kept its format version when includes were added to it, so a package written before
    // that change is detected by running out of data rather than by the version check.
    TEST(GameData_RejectsAPackageThatEndsBeforeItsShaderIncludes)
    {
        const Testing::TempDir dir;
        const auto path = dir.File("older-build.eppak");

        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.PackedShaders.emplace("geometry", PackedShaderData{ {
            { nvrhi::ShaderType::Vertex, "vertex-source" },
        } });
        data.PackedShaderIncludes.emplace("Includes/platform.hlsli", "platform-source");
        REQUIRE CHECK(data.Serialize(path));

        std::vector<char> bytes = FS::ReadBytes(path);
        const auto includeBlockSize = sizeof(uint32_t) + 2 * sizeof(uint64_t) + std::string("Includes/platform.hlsli").size()
            + std::string("platform-source").size();
        REQUIRE CHECK(bytes.size() > includeBlockSize);
        bytes.resize(bytes.size() - includeBlockSize);
        REQUIRE CHECK(FS::WriteBytes(path, bytes, true));

        GameData loaded;
        CHECK(!loaded.Deserialize(path));
    }

    TEST(GameData_RejectsUnknownPackageFormat)
    {
        const Testing::TempDir dir;
        const auto path = dir.File("bad-package-format.eppak");

        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        REQUIRE CHECK(data.Serialize(path));

        std::vector<char> bytes = FS::ReadBytes(path);
        REQUIRE CHECK(!bytes.empty());
        bytes.front() = static_cast<char>(bytes.front() + 1);
        REQUIRE CHECK(FS::WriteBytes(path, bytes, true));

        GameData loaded;
        CHECK(!loaded.Deserialize(path));
    }

    TEST(GameData_RejectsUnknownShaderFormat)
    {
        const Testing::TempDir dir;
        const auto path = dir.File("bad-shader-format.eppak");

        GameData data;
        data.ProjectName = "Game";
        data.StartScene = AssetHandle(100);
        data.PackedShaders.emplace("geometry", PackedShaderData{ {
            { nvrhi::ShaderType::Vertex, "vertex-source" },
        } });
        REQUIRE CHECK(data.Serialize(path));

        // With no registry or packed assets the shader magic occurs exactly once, so corrupting
        // the first occurrence targets the shader block without hard-coding a byte offset.
        std::vector<char> bytes = FS::ReadBytes(path);
        REQUIRE CHECK(!bytes.empty());
        const auto magic = PackFormat::Shader.Magic;
        const auto* magicBytes = reinterpret_cast<const char*>(&magic);
        const auto position = std::search(bytes.begin(), bytes.end(), magicBytes, magicBytes + sizeof(magic));
        REQUIRE CHECK(position != bytes.end());
        *position = static_cast<char>(*position + 1);
        REQUIRE CHECK(FS::WriteBytes(path, bytes, true));

        GameData loaded;
        CHECK(!loaded.Deserialize(path));
    }
}
