#pragma once

#include "Asset/AssetManagerBase.h"
#include "Project/Project.h"

namespace Eppo
{
	class AssetManager
	{
	public:
		static bool CreateAsset(Ref<Asset> asset, const std::filesystem::path& filepath)
		{
			return Project::GetActive()->GetAssetManager()->CreateAsset(asset, filepath);
		}

		template<typename T>
		static Ref<T> GetAsset(AssetHandle handle)
		{
			Ref<Asset> asset = Project::GetActive()->GetAssetManager()->GetAsset(handle);
			return std::static_pointer_cast<T>(asset);
		}

		static bool IsAssetHandleValid(AssetHandle handle)
		{
			return Project::GetActive()->GetAssetManager()->IsAssetHandleValid(handle);
		}

		static bool IsAssetLoaded(AssetHandle handle)
		{
			return Project::GetActive()->GetAssetManager()->IsAssetLoaded(handle);
		}

		static AssetType GetAssetType(AssetHandle handle)
		{
			return Project::GetActive()->GetAssetManager()->GetAssetType(handle);
		}
	};
}
