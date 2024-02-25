#pragma once

#include "Asset/Asset.h"
#include "Core/Buffer.h"
#include "Renderer/Image.h"

namespace Eppo
{
	class Texture : public Asset
	{
	public:
		virtual ~Texture() {};

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Texture; }

		static Ref<Texture> Create(const std::filesystem::path& filepath);
		static Ref<Texture> Create(uint32_t width, uint32_t height, ImageFormat format, void* data);
	
	protected:
		Texture(const std::filesystem::path& filepath);
		Texture(uint32_t width, uint32_t height);

	protected:
		std::filesystem::path m_Filepath;

		uint32_t m_Width;
		uint32_t m_Height;

		Buffer m_ImageData;
	};
}
