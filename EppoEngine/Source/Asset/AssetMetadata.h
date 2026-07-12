#pragma once

#include "Asset/Asset.h"
#include "Utility/Filesystem.h"

namespace Eppo
{
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type = AssetType::None;
		std::filesystem::path Filepath;
		bool IsRuntimeAsset = false;

		[[nodiscard]] auto IsValid() const -> bool
		{
			return static_cast<bool>(Handle);
		}

		[[nodiscard]] auto GetName() const -> std::string
		{
			return Filepath.filename().string();
		}
	};
}