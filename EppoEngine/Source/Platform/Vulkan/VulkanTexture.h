#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Texture.h"

namespace Eppo
{
    class VulkanTexture : public Texture
    {
    public:
        VulkanTexture(const std::filesystem::path& filepath);
        VulkanTexture(uint32_t width, uint32_t height, ImageFormat format, void* data);
        virtual ~VulkanTexture();

        Ref<Image> GetImage() const { return m_Image; }

        VkDescriptorSet& GetDescriptorSet() { return m_DescriptorSet; }

    private:
		Ref<Image> m_Image;
		VkDescriptorSet m_DescriptorSet;
    };
}
