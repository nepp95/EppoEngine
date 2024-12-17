#pragma once

#include "Asset/Asset.h"

namespace Eppo
{
	class AssetManagerBase
	{
	public:
		virtual ~AssetManagerBase() = default;
		virtual bool CreateAsset(Ref<Asset> asset, const std::filesystem::path& filepath) = 0;
		virtual Ref<Asset> GetAsset(AssetHandle handle) = 0;
		[[nodiscard]] virtual bool IsAssetHandleValid(AssetHandle handle) const = 0;
		[[nodiscard]] virtual bool IsAssetLoaded(AssetHandle handle) const = 0;
		[[nodiscard]] virtual AssetType GetAssetType(AssetHandle handle) const = 0;
	};
}
