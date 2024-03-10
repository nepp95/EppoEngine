#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Image.h"

namespace Eppo
{
	struct VulkanImageInfo
	{
		VkImage Image;
		VkImageView ImageView;
		VkImageLayout ImageLayout;
		VkSampler Sampler;
		VmaAllocation Allocation;
	};

	class VulkanImage : public Image
	{
	public:
		VulkanImage(const ImageSpecification& specification);
		~VulkanImage();

		void Release();

		VulkanImageInfo& GetImageInfo() { return m_Info; }
		VkImageView GetImageView() const { return m_Info.ImageView; }

		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }

		const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorImageInfo; }

	private:
		ImageSpecification m_Specification;

		VulkanImageInfo m_Info;
		VkDescriptorImageInfo m_DescriptorImageInfo;
	};

	namespace Utils
	{
		inline VkFormat FindSupportedDepthFormat()
		{
			Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
			Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();

			std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT };

			for (const auto& format : depthFormats)
			{
				VkFormatProperties properties;
				vkGetPhysicalDeviceFormatProperties(physicalDevice->GetNativeDevice(), format, &properties);
				if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					return format;
			}
		}

		inline VkFormat ImageFormatToVkFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::None:			return VK_FORMAT_UNDEFINED;
				case ImageFormat::RGBA8:		return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::RGBA8_UNORM:	return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::Depth:		return FindSupportedDepthFormat();
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

		inline ImageFormat VkFormatToImageFormat(VkFormat format)
		{
			switch (format)
			{
				case VK_FORMAT_UNDEFINED:		return ImageFormat::None;
				case VK_FORMAT_R8G8B8A8_SRGB:	return ImageFormat::RGBA8;
				case VK_FORMAT_R8G8B8A8_UNORM:	return ImageFormat::RGBA8_UNORM;
				case VK_FORMAT_D32_SFLOAT:		return ImageFormat::Depth;
			}

			EPPO_ASSERT(false);
			return ImageFormat::None;
		}
	}
}
