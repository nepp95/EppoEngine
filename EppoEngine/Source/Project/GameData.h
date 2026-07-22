#pragma once

#include "Asset/AssetMetadata.h"

namespace Eppo
{
	struct GameData
	{
		static constexpr std::string_view Filename = "Game.eppak";

		std::string ProjectName;
		AssetHandle StartScene = 0;
		std::map<AssetHandle, AssetMetadata> AssetRegistry;
		std::map<AssetHandle, PackedAssetData> PackedAssets;

		auto Serialize(const std::filesystem::path& path) const -> bool;
		auto Deserialize(const std::filesystem::path& path) -> bool;
	};
}
