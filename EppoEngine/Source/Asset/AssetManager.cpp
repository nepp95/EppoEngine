#include "pch.h"
#include "AssetManager.h"

#include "Renderer/Texture.h"

namespace Eppo
{
	static AssetMetadata s_NullMetadata;

	AssetManager& AssetManager::Get()
	{
		static AssetManager a;
		return a;
	}

	bool AssetManager::LoadData(const std::filesystem::path& filepath, Ref<Asset>& asset)
	{
		const AssetMetadata& metadata = GetMetadata(filepath);

		switch (metadata.Type)
		{
			case AssetType::Texture:
			{
				asset = CreateRef<Texture>(filepath);
				return true;
			}
		}

		EPPO_ASSERT(false);
		return false;
	}

	bool AssetManager::IsAssetLoaded(AssetHandle handle)
	{
		return m_Assets.find(handle) != m_Assets.end();
	}

	bool AssetManager::IsAssetLoaded(const std::filesystem::path& filepath)
	{
		AssetMetadata& metadata = GetMetadata(filepath);
		return IsAssetLoaded(metadata.Handle);
	}

	AssetMetadata& AssetManager::GetMetadata(AssetHandle handle)
	{
		auto it = m_AssetData.find(handle);
		if (it != m_AssetData.end())
			return it->second;

		return s_NullMetadata;
	}

	AssetMetadata& AssetManager::GetMetadata(const std::filesystem::path& filepath)
	{
		for (auto& [handle, metadata] : m_AssetData)
		{
			if (metadata.Filepath == filepath)
				return metadata;
		}

		return s_NullMetadata;
	}
}
