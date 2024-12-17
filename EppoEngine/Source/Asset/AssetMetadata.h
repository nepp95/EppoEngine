#pragma once

#include "Asset/Asset.h"
#include "Core/Filesystem.h"

namespace Eppo
{
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type = AssetType::None;

		std::filesystem::path Filepath;

		[[nodiscard]] bool IsValid() const
		{
			return Handle && Filesystem::Exists(Filepath);
		}

		[[nodiscard]] std::string GetName() const
		{
			return Filepath.filename().string();
		}
	};
}
