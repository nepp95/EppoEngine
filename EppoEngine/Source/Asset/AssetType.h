#pragma once

namespace Eppo
{
    enum class AssetType : uint8_t
    {
        None = 0,
        Mesh,
        Scene,
        Texture,
        Script,
    };

    namespace Utils
    {
        constexpr auto AssetTypeFromString(const std::string_view assetType) -> AssetType
        {
            if (assetType == "None")
                return AssetType::None;
            if (assetType == "Mesh")
                return AssetType::Mesh;
            if (assetType == "Scene")
                return AssetType::Scene;
            if (assetType == "Texture")
                return AssetType::Texture;
            if (assetType == "Script")
                return AssetType::Script;

            EP_ASSERT(false);
            return AssetType::None;
        }

        constexpr auto AssetTypeToString(const AssetType& type) -> std::string
        {
            switch (type)
            {
                case AssetType::None:
                    return "None";
                case AssetType::Mesh:
                    return "Mesh";
                case AssetType::Scene:
                    return "Scene";
                case AssetType::Texture:
                    return "Texture";
                case AssetType::Script:
                    return "Script";
                default:
                {
                    EP_ASSERT(false);
                    return "None";
                }
            }
        }
    }
}