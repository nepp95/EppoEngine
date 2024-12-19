#include "ThumbnailCache.h"

#include <chrono>

namespace Eppo
{
	ThumbnailCache::ThumbnailCache()
	{
		m_AssetTypeThumbnails[AssetType::None] = Image::Create(ImageSpecification("Resources/Textures/Icons/Unknown.png"));
		m_AssetTypeThumbnails[AssetType::Mesh] = Image::Create(ImageSpecification("Resources/Textures/Icons/Mesh.png"));
		m_AssetTypeThumbnails[AssetType::Scene] = Image::Create(ImageSpecification("Resources/Textures/Icons/Scene.png"));
		m_AssetTypeThumbnails[AssetType::Script] = Image::Create(ImageSpecification("Resources/Textures/Icons/Script.png"));
		m_AssetTypeThumbnails[AssetType::Texture] = Image::Create(ImageSpecification("Resources/Textures/Icons/Texture.png"));
	}

	Ref<Image> ThumbnailCache::GetOrCreateThumbnail(const std::filesystem::path& filepath)
	{
		const auto absPath = Project::GetAssetFilepath(filepath);
		const std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(absPath);
		uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(lastWriteTime.time_since_epoch()).count();

		return nullptr;
	}

	Ref<Image> ThumbnailCache::GetOrCreateThumbnail(const AssetType type)
	{
		if (m_AssetTypeThumbnails.find(type) == m_AssetTypeThumbnails.end())
			return m_AssetTypeThumbnails.at(AssetType::None);

		return m_AssetTypeThumbnails.at(type);
	}
}
