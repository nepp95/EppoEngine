#pragma once

#include "Asset/Asset.h"
#include "Core/Filesystem.h"

namespace Eppo
{
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type;

		std::filesystem::path Filepath;

		bool IsValid() const
		{
			return Handle && Filesystem::Exists(Filepath);
		}
	};
}
