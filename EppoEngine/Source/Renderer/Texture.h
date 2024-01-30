#pragma once

#include "Asset/Asset.h"
#include "Renderer/Image.h"

namespace Eppo
{
	class Texture : public Asset
	{
	public:
		virtual ~Texture() {};

		virtual Ref<Image> GetImage() const = 0;
		
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// Asset
		static AssetType GetStaticType() { return AssetType::Texture; }

		static Ref<Texture> Create(const std::filesystem::path& filepath);
		static Ref<Texture> Create(uint32_t width, uint32_t height, ImageFormat format, void* data);
	};
}
