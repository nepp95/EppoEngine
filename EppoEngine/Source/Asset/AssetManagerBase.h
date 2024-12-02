#pragma once

#include "Asset/Asset.h"

namespace Eppo
{
	class AssetManagerBase
	{
	public:
		virtual bool CreateAsset(Ref<Asset> asset, const std::filesystem::path& filepath) = 0;
		virtual Ref<Asset> GetAsset(AssetHandle handle) = 0;
		virtual bool IsAssetHandleValid(AssetHandle handle) const = 0;
		virtual bool IsAssetLoaded(AssetHandle handle) const = 0;
		virtual AssetType GetAssetType(AssetHandle handle) const = 0;
	};
}
