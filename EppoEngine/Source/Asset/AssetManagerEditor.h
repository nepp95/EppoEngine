#pragma once

#include "Asset/AssetManagerBase.h"
#include "Asset/AssetMetadata.h"

#include <map>

namespace Eppo
{
	class AssetManagerEditor : public AssetManagerBase
	{
	public:
		bool CreateAsset(Ref<Asset> asset, const std::filesystem::path& filepath) override;
		Ref<Asset> GetAsset(AssetHandle handle) override;

		bool IsAssetHandleValid(AssetHandle handle) const override;
		bool IsAssetLoaded(AssetHandle handle) const override;
		AssetType GetAssetType(AssetHandle handle) const override;

		Ref<Asset> ImportAsset(const std::filesystem::path& filepath);

		const AssetMetadata& GetMetadata(AssetHandle handle) const;
		const std::filesystem::path& GetFilepath(AssetHandle handle) const;

		const std::map<AssetHandle, AssetMetadata>& GetAssetRegistry() const { return m_AssetData; }

		void SerializeAssetRegistry() const;
		bool DeserializeAssetRegistry();

	private:
		std::map<AssetHandle, AssetMetadata> m_AssetData;
		std::map<AssetHandle, Ref<Asset>> m_Assets;
	};
}
