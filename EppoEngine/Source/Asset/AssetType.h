#pragma once

#include <string>

namespace Eppo
{
	enum class AssetType
	{
		None = 0,
		Texture
	};

	namespace Utils
	{
		inline AssetType AssetTypeFromString(const std::string& assetType)
		{
			if (assetType == "None")		return AssetType::None;
			if (assetType == "Texture")		return AssetType::Texture;

			EPPO_ASSERT(false);
			return AssetType::None;
		}

		inline std::string AssetTypeToString(AssetType type)
		{
			switch (type)
			{
				case AssetType::None:		return "None";
				case AssetType::Texture:	return "Texture";
			}

			EPPO_ASSERT(false);
			return "None";
		}
	}
}
