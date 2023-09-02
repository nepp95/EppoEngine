#pragma once

#include "Asset/AssetMetadata.h"

namespace Eppo
{
	class AssetManager
	{
	public:
		static AssetManager& Get();

		template<typename T>
		Ref<T> LoadAsset(const std::filesystem::path& filepath)
		{
			static_assert(std::is_base_of_v<Asset, T>, "Class is not based on Asset!");

			if (!Filesystem::Exists(filepath))
				return nullptr;

			// Check if asset exists
			Ref<Asset> asset = nullptr;
			const AssetMetadata& metadata = GetMetadata(filepath);

			if (metadata.IsValid() && IsAssetLoaded(metadata.Handle))
				asset = m_Assets.at(metadata.Handle);
			else
			{
				// Load actual asset
				AssetMetadata newMetadata;
				newMetadata.Handle = AssetHandle();
				newMetadata.Type = T::GetStaticType();
				newMetadata.Filepath = filepath;

				m_AssetData[newMetadata.Handle] = newMetadata;

				if (!LoadData(newMetadata.Filepath, asset))
					return nullptr;

				asset->Handle = newMetadata.Handle;
				m_Assets[newMetadata.Handle] = asset;
			}

			// TODO: Safe?
			return std::dynamic_pointer_cast<T>(asset);
		}

		template<typename T>
		Ref<T> GetAsset(AssetHandle handle)
		{
			Ref<Asset> asset = nullptr;

			if (!IsAssetLoaded(handle))
				return nullptr;

			asset = m_Assets.at(handle);
			
			return dynamic_pointer_cast<T>(asset);
		}

		template<typename T>
		Ref<T> GetAsset(const std::filesystem::path& filepath)
		{
			return GetAsset<T>(GetMetadata(filepath).Handle);
		}

	private:
		AssetManager() = default;
		
		bool LoadData(const std::filesystem::path& filepath, Ref<Asset>& asset);

		bool IsAssetLoaded(AssetHandle handle);
		bool IsAssetLoaded(const std::filesystem::path& filepath);
		AssetMetadata& GetMetadata(AssetHandle handle);
		AssetMetadata& GetMetadata(const std::filesystem::path& filepath);

	private:
		std::unordered_map<AssetHandle, Ref<Asset>> m_Assets;
		std::unordered_map<AssetHandle, AssetMetadata> m_AssetData;
	};
}
