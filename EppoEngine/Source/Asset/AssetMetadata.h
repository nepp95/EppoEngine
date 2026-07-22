#pragma once

#include "Asset/Asset.h"

namespace Eppo
{
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type = AssetType::None;
		std::filesystem::path Filepath;
		bool IsRuntimeAsset = false;

		[[nodiscard]] auto IsValid() const -> bool
		{
			return static_cast<bool>(Handle);
		}

		[[nodiscard]] auto GetName() const -> std::string
		{
			return Filepath.filename().string();
		}
	};

	struct PackedAssetData
	{
		AssetType Type = AssetType::None;
		std::vector<uint8_t> Payload;
	};
}
