#pragma once

#include "Asset/AssetMetadata.h"

#include <unordered_map>

namespace Eppo
{
	class AssetManager
	{
	public:
		static AssetManager& Get();

		void Shutdown();

		template<typename T>
		Ref<T> LoadAsset(const std::filesystem::path& filepath)
		{
			EPPO_PROFILE_FUNCTION("AssetManager::LoadAsset");

			static_assert(std::is_base_of_v<Asset, T>, "Class is not based on Asset!");

			if (!Filesystem::Exists(filepath))
				return nullptr;

			// Check if asset exists
			Ref<Asset> asset = nullptr;
			const AssetMetadata& metadata = GetMetadata(filepath);

			if (IsAssetLoaded(metadata.Handle))
				asset = m_Assets.at(metadata.Handle);
			else
			{
				// Load actual asset
				AssetHandle handle;

				if (!metadata.IsValid())
				{
					AssetMetadata newMetadata;
					newMetadata.Handle = handle;
					newMetadata.Type = T::GetStaticType();
					newMetadata.Filepath = filepath;

					m_AssetData[handle] = newMetadata;
				}
				else
				{
					handle = metadata.Handle;
				}

				if (!LoadData(filepath, asset))
					return nullptr;

				m_Assets[handle] = asset;
			}

			WriteRegistry();

			// TODO: Safe?
			return std::dynamic_pointer_cast<T>(asset);
		}

		template<typename T>
		Ref<T> GetAsset(AssetHandle handle)
		{
			EPPO_PROFILE_FUNCTION("AssetManager::GetAsset");

			Ref<Asset> asset = nullptr;

			if (!IsAssetLoaded(handle))
				return nullptr;

			asset = m_Assets.at(handle);
			
			return std::dynamic_pointer_cast<T>(asset);
		}

		template<typename T>
		Ref<T> GetAsset(const std::filesystem::path& filepath)
		{
			EPPO_PROFILE_FUNCTION("AssetManager::GetAsset");

			return GetAsset<T>(GetMetadata(filepath).Handle);
		}

		bool IsAssetLoaded(AssetHandle handle);
		bool IsAssetLoaded(const std::filesystem::path& filepath);
		AssetMetadata& GetMetadata(AssetHandle handle);
		AssetMetadata& GetMetadata(const std::filesystem::path& filepath);

	private:
		AssetManager();
		
		void LoadAsset(const AssetMetadata& metadata);
		bool LoadData(const std::filesystem::path& filepath, Ref<Asset>& asset);
		void LoadRegistry();
		void WriteRegistry();

	private:
		std::unordered_map<AssetHandle, Ref<Asset>> m_Assets;
		std::unordered_map<AssetHandle, AssetMetadata> m_AssetData;
	};
}
