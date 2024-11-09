#include "ThumbnailCache.h"

#include <chrono>

namespace Eppo
{
	ThumbnailCache::ThumbnailCache()
	{
		m_AssetTypeThumbnails[AssetType::None] = CreateRef<Texture>(TextureSpecification("Resources/Textures/Icons/Unknown.png"));
		m_AssetTypeThumbnails[AssetType::Mesh] = CreateRef<Texture>(TextureSpecification("Resources/Textures/Icons/Mesh.png"));
		m_AssetTypeThumbnails[AssetType::Scene] = CreateRef<Texture>(TextureSpecification("Resources/Textures/Icons/Scene.png"));
		m_AssetTypeThumbnails[AssetType::Script] = CreateRef<Texture>(TextureSpecification("Resources/Textures/Icons/Script.png"));
		m_AssetTypeThumbnails[AssetType::Texture] = CreateRef<Texture>(TextureSpecification("Resources/Textures/Icons/Texture.png"));
	}

	Ref<Texture> ThumbnailCache::GetOrCreateThumbnail(const std::filesystem::path& filepath)
	{
		auto absPath = Project::GetAssetFilepath(filepath);
		std::filesystem::file_time_type lastWriteTime = std::filesystem::last_write_time(absPath);
		uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(lastWriteTime.time_since_epoch()).count();

		return nullptr;
	}

	Ref<Texture> ThumbnailCache::GetOrCreateThumbnail(AssetType type)
	{
		if (m_AssetTypeThumbnails.find(type) == m_AssetTypeThumbnails.end())
			return m_AssetTypeThumbnails.at(AssetType::None);

		return m_AssetTypeThumbnails.at(type);
	}
}
