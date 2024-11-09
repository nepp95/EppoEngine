#pragma once

#include <EppoEngine.h>

namespace Eppo
{
	struct Thumbnail
	{
		uint64_t Timestamp;
		Ref<Texture> Image;
	};

	class ThumbnailCache
	{
	public:
		ThumbnailCache();

		Ref<Texture> GetOrCreateThumbnail(const std::filesystem::path& filepath);
		Ref<Texture> GetOrCreateThumbnail(AssetType type);

	private:
		std::unordered_map<std::filesystem::path, Thumbnail> m_AssetThumbnails;
		std::unordered_map<AssetType, Ref<Texture>> m_AssetTypeThumbnails;
	};
}
