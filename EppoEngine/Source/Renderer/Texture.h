#pragma once

#include "Asset/Asset.h"
#include "Core/Buffer.h"
#include "Renderer/Image.h"

typedef struct VkDescriptorSet_T* VkDescriptorSet;

namespace Eppo
{
	class Texture : public Asset
	{
	public:
		Texture(const std::filesystem::path& filepath);
		Texture(uint32_t width, uint32_t height, ImageFormat format, void* data);
		~Texture();

		Ref<Image> GetImage() const { return m_Image; }
		VkDescriptorSet& GetDescriptorSet() { return m_DescriptorSet; }

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Texture; }

	private:
		std::filesystem::path m_Filepath;
		Ref<Image> m_Image;

		Buffer m_ImageData;

		VkDescriptorSet m_DescriptorSet;

		uint32_t m_Width;
		uint32_t m_Height;
	};
}
