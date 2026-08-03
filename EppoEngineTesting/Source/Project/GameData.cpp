#include "Support/EppoTest.h"
#include "Support/TempDir.h"

#include "Asset/PackFormat.h"
#include "Project/GameData.h"

#include <algorithm>

using namespace Eppo;

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

TEST(Project, GameData_FilenameIsFixed)
{
    EXPECT_EQ(std::string("Game.eppak"), std::string(GameData::Filename));
}

TEST(Project, GameData_DeterministicRoundTripPreservesDeploymentData)
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

    EP_REQUIRE(first.Serialize(firstPath));
    EP_REQUIRE(second.Serialize(secondPath));
    const auto firstBytes = FS::ReadBytes(firstPath);
    const auto secondBytes = FS::ReadBytes(secondPath);
    EXPECT_EQ(firstBytes.size(), secondBytes.size());
    EP_EXPECT_ARRAY_EQ(firstBytes.data(), secondBytes.data(), firstBytes.size());

    GameData loaded;
    EP_REQUIRE(loaded.Deserialize(firstPath));
    EXPECT_EQ(std::string("TestGame"), loaded.ProjectName);
    EXPECT_EQ(100, static_cast<uint64_t>(loaded.StartScene));
    EXPECT_EQ(2, loaded.AssetRegistry.size());
    EXPECT_EQ(std::string("Scenes/start.epscene"), loaded.AssetRegistry.at(100).Filepath.generic_string());
    EXPECT_EQ(std::string("Textures/image.png"), loaded.AssetRegistry.at(200).Filepath.generic_string());
    EXPECT_EQ(1, loaded.PackedAssets.size());
    EXPECT_TRUE(loaded.PackedAssets.at(100).Type == AssetType::Scene);
    EP_EXPECT_ARRAY_EQ(
        first.PackedAssets.at(100).Payload.Data, loaded.PackedAssets.at(100).Payload.Data, first.PackedAssets.at(100).Payload.Size
    );
}

TEST(Project, GameData_ExcludesRuntimeGeneratedRegistryEntries)
{
    const Testing::TempDir dir;
    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    data.AssetRegistry.emplace(1, AssetMetadata{ AssetHandle(1), AssetType::Mesh, {}, true });
    data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
    data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });
    EP_REQUIRE(data.Serialize(dir.File("runtime-assets.eppak")));

    GameData loaded;
    EP_REQUIRE(loaded.Deserialize(dir.File("runtime-assets.eppak")));
    EXPECT_EQ(1, loaded.AssetRegistry.size());
    EXPECT_TRUE(loaded.AssetRegistry.contains(100));
    EXPECT_TRUE(!loaded.AssetRegistry.contains(1));
}

TEST(Project, GameData_RoundTripPreservesPackedShaderSources)
{
    const Testing::TempDir dir;
    const auto path = dir.File("shaders.eppak");

    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
    data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });
    data.PackedShaders.emplace("geometry", "geometry-source");
    data.PackedShaders.emplace("composite", "composite-source");
    EP_REQUIRE(data.Serialize(path));

    GameData loaded;
    EP_REQUIRE(loaded.Deserialize(path));
    EXPECT_EQ(2, loaded.PackedShaders.size());
    EXPECT_EQ(std::string("geometry-source"), loaded.PackedShaders.at("geometry"));
    EXPECT_EQ(std::string("composite-source"), loaded.PackedShaders.at("composite"));
}

TEST(Project, GameData_RoundTripAllowsNoPackedShaders)
{
    const Testing::TempDir dir;
    const auto path = dir.File("no-shaders.eppak");

    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    data.AssetRegistry.emplace(100, AssetMetadata{ AssetHandle(100), AssetType::Scene, "Scenes/start.epscene" });
    data.PackedAssets.emplace(100, PackedAssetData{ AssetType::Scene, MakePayload(100) });
    EP_REQUIRE(data.Serialize(path));

    GameData loaded;
    EP_REQUIRE(loaded.Deserialize(path));
    EXPECT_TRUE(loaded.PackedShaders.empty());
    EXPECT_TRUE(loaded.PackedShaderIncludes.empty());
}

TEST(Project, GameData_RoundTripPreservesPackedShaderIncludes)
{
    const Testing::TempDir dir;
    const auto path = dir.File("includes.eppak");

    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    data.PackedShaders.emplace("geometry", "#include \"Includes/platform.hlsli\"\nvertex-source");
    // Keyed by the path relative to Resources/Shaders, which is exactly what an #include names.
    data.PackedShaderIncludes.emplace("Includes/platform.hlsli", "platform-source");
    data.PackedShaderIncludes.emplace("Includes/lighting.hlsli", "lighting-source");
    EP_REQUIRE(data.Serialize(path));

    GameData loaded;
    EP_REQUIRE(loaded.Deserialize(path));
    EXPECT_EQ(2, loaded.PackedShaderIncludes.size());
    EXPECT_EQ(std::string("platform-source"), loaded.PackedShaderIncludes.at("Includes/platform.hlsli"));
    EXPECT_EQ(std::string("lighting-source"), loaded.PackedShaderIncludes.at("Includes/lighting.hlsli"));
}

// The shader block kept its format version when includes were added to it, so a package written before
// that change is detected by running out of data rather than by the version check.
TEST(Project, GameData_RejectsAPackageThatEndsBeforeItsShaderIncludes)
{
    const Testing::TempDir dir;
    const auto path = dir.File("older-build.eppak");

    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    data.PackedShaders.emplace("geometry", "vertex-source");
    data.PackedShaderIncludes.emplace("Includes/platform.hlsli", "platform-source");
    EP_REQUIRE(data.Serialize(path));

    std::vector<char> bytes = FS::ReadBytes(path);
    const auto includeBlockSize =
        sizeof(uint32_t) + 2 * sizeof(uint64_t) + std::string("Includes/platform.hlsli").size() + std::string("platform-source").size();
    EP_REQUIRE(bytes.size() > includeBlockSize);
    bytes.resize(bytes.size() - includeBlockSize);
    EP_REQUIRE(FS::WriteBytes(path, bytes, true));

    GameData loaded;
    EXPECT_TRUE(!loaded.Deserialize(path));
}

TEST(Project, GameData_RejectsUnknownPackageFormat)
{
    const Testing::TempDir dir;
    const auto path = dir.File("bad-package-format.eppak");

    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    EP_REQUIRE(data.Serialize(path));

    std::vector<char> bytes = FS::ReadBytes(path);
    EP_REQUIRE(!bytes.empty());
    bytes.front() = static_cast<char>(bytes.front() + 1);
    EP_REQUIRE(FS::WriteBytes(path, bytes, true));

    GameData loaded;
    EXPECT_TRUE(!loaded.Deserialize(path));
}

TEST(Project, GameData_RejectsUnknownShaderFormat)
{
    const Testing::TempDir dir;
    const auto path = dir.File("bad-shader-format.eppak");

    GameData data;
    data.ProjectName = "Game";
    data.StartScene = AssetHandle(100);
    data.PackedShaders.emplace("geometry", "vertex-source");
    EP_REQUIRE(data.Serialize(path));

    // With no registry or packed assets the shader magic occurs exactly once, so corrupting
    // the first occurrence targets the shader block without hard-coding a byte offset.
    std::vector<char> bytes = FS::ReadBytes(path);
    EP_REQUIRE(!bytes.empty());
    const auto magic = PackFormat::Shader.Magic;
    const auto* magicBytes = reinterpret_cast<const char*>(&magic);
    const auto position = std::search(bytes.begin(), bytes.end(), magicBytes, magicBytes + sizeof(magic));
    EP_REQUIRE(position != bytes.end());
    *position = static_cast<char>(*position + 1);
    EP_REQUIRE(FS::WriteBytes(path, bytes, true));

    GameData loaded;
    EXPECT_TRUE(!loaded.Deserialize(path));
}
