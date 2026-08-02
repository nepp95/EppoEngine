#pragma once

#include "Asset/AssetMetadata.h"
#include "Renderer/Shader.h"

namespace Eppo
{
    /// [Package header]
    ///     uint32   magic
    ///     uint32   version
    ///     string   projectName
    ///     uint64   startScene
    ///
    /// [Asset registry]
    ///     uint32   registryCount
    ///     registryCount x:
    ///         uint64   handle
    ///         uint8    type
    ///         string   filepath
    ///
    /// [Packed assets]
    ///     uint32   packedAssetCount
    ///     packedAssetCount x:
    ///         uint64   handle
    ///         uint8    type
    ///         uint64   payloadSize
    ///         uint8[payloadSize] payload
    ///
    /// [Shaders]
    ///     uint32   magic
    ///     uint32   version
    ///     uint32   shaderCount
    ///     shaderCount x:
    ///         string   name
    ///         string   source
    ///     uint32   includeCount
    ///     includeCount x:
    ///         string   path
    ///         string   source
    struct GameData
    {
        static constexpr std::string_view Filename = "Game.eppak";

        std::string ProjectName;
        AssetHandle StartScene = 0;

        std::map<AssetHandle, AssetMetadata> AssetRegistry;
        std::map<AssetHandle, PackedAssetData> PackedAssets;
        std::map<std::string, std::string> PackedShaders;
        // Keyed by path relative to Resources/Shaders, matching what the shader sources #include.
        std::map<std::string, std::string> PackedShaderIncludes;

        // Serializes a project' data for use in a deployed application
        [[nodiscard]] auto Serialize(const std::filesystem::path& path) const -> bool;

        // Deserializes a project' data for use in a deployed application
        auto Deserialize(const std::filesystem::path& path) -> bool;
    };
}
