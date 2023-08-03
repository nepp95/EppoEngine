#include "pch.h"
#include "Image.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	namespace Utils
	{
		static VkFormat ImageFormatToVkFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::RGBA8:		return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::RGBA8_UNORM:	return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::Depth:		return VK_FORMAT_D32_SFLOAT;
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}
	}

	Image::Image(const ImageSpecification& specification)
		: m_Specification(specification)
	{
		VkImageUsageFlags usageFlags{};

		if (m_Specification.Usage == ImageUsage::Texture)
			usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		else
		{
			if (m_Specification.Format == ImageFormat::Depth)
				usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			else
				usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		// Image
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_Specification.Width;
		imageInfo.extent.height = m_Specification.Height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = Utils::ImageFormatToVkFormat(m_Specification.Format);
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usageFlags;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;

		m_Allocation = Allocator::AllocateImage(m_Image, imageInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Image View
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = Utils::ImageFormatToVkFormat(m_Specification.Format);
		viewInfo.subresourceRange = {};
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView), "Failed to create image view!");
	}

	Image::~Image()
	{
		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		vkDestroyImageView(device, m_ImageView, nullptr);
		Allocator::DestroyImage(m_Image, m_Allocation);
	}
}
