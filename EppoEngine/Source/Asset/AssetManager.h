#pragma once

#include "Asset/AssetManagerBase.h"
#include "Project/Project.h"

namespace Eppo
{
	class AssetManager
	{
	public:
		static bool CreateAsset(const Ref<Asset>& asset, const std::filesystem::path& filepath)
		{
			return Project::GetActive()->GetAssetManager()->CreateAsset(asset, filepath);
		}

		template<typename T>
		static Ref<T> GetAsset(const AssetHandle handle)
		{
			const Ref<Asset> asset = Project::GetActive()->GetAssetManager()->GetAsset(handle);
			return std::static_pointer_cast<T>(asset);
		}

		static bool IsAssetHandleValid(const AssetHandle handle)
		{
			return Project::GetActive()->GetAssetManager()->IsAssetHandleValid(handle);
		}

		static bool IsAssetLoaded(const AssetHandle handle)
		{
			return Project::GetActive()->GetAssetManager()->IsAssetLoaded(handle);
		}

		static AssetType GetAssetType(const AssetHandle handle)
		{
			return Project::GetActive()->GetAssetManager()->GetAssetType(handle);
		}
	};
}
