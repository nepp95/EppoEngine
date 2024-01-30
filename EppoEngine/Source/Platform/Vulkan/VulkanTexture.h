#pragma once

#include "Core/Buffer.h"
#include "Renderer/Texture.h"

typedef struct VkDescriptorSet_T* VkDescriptorSet;

namespace Eppo
{
    class VulkanTexture : public Texture
    {
    public:
        VulkanTexture(const std::filesystem::path& filepath);
        VulkanTexture(uint32_t width, uint32_t height, ImageFormat format, void* data);
        virtual ~VulkanTexture();

        Ref<Image> GetImage() const override { return m_Image; }

        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

        VkDescriptorSet& GetDescriptorSet() { return m_DescriptorSet; }

    private:
        std::filesystem::path m_Filepath;
		Ref<Image> m_Image;

		Buffer m_ImageData;

		VkDescriptorSet m_DescriptorSet;

		uint32_t m_Width;
		uint32_t m_Height;
    };
}
