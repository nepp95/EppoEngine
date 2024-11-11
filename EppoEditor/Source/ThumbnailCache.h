#pragma once

#include <EppoEngine.h>

namespace Eppo
{
	struct Thumbnail
	{
		uint64_t Timestamp;
		Ref<Image> Image;
	};

	class ThumbnailCache
	{
	public:
		ThumbnailCache();

		Ref<Image> GetOrCreateThumbnail(const std::filesystem::path& filepath);
		Ref<Image> GetOrCreateThumbnail(AssetType type);

	private:
		std::unordered_map<std::filesystem::path, Thumbnail> m_AssetThumbnails;
		std::unordered_map<AssetType, Ref<Image>> m_AssetTypeThumbnails;
	};
}
