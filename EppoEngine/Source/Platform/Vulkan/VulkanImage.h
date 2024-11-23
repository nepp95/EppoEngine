#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Image.h"

namespace Eppo
{
	struct ImageInfo
	{
		VkImage Image = nullptr;
		VkImageView ImageView = nullptr;
		VkImageLayout ImageLayout;
		VkSampler Sampler = nullptr;
		VmaAllocation Allocation = nullptr;
	};

	class VulkanImage : public Image
	{
	public:
		explicit VulkanImage(const ImageSpecification& specification);
		~VulkanImage() final;

		void SetData(void* data, uint32_t channels = 4) override;
		void Release() override;

		const ImageSpecification& GetSpecification() const override { return m_Specification; }

		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }

		ImageInfo& GetImageInfo() { return m_ImageInfo; }

		static void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout srcLayout, VkImageLayout dstLayout);
		static VkImageAspectFlags GetImageAspectFlags(VkImageLayout layout);

	private:
		ImageSpecification m_Specification;
		ImageInfo m_ImageInfo;

		void* m_ImageData = nullptr;
	};

	namespace Utils
	{
		inline VkFormat FindSupportedDepthFormat()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();

			std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT };

			for (const auto& format : depthFormats)
			{
				VkFormatProperties properties;
				vkGetPhysicalDeviceFormatProperties(physicalDevice->GetNativeDevice(), format, &properties);
				if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return format;
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

		inline VkFormat ImageFormatToVkFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::None:     return VK_FORMAT_UNDEFINED;
				case ImageFormat::RGBA8:    return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::Depth:    return FindSupportedDepthFormat();
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}
	}
}
