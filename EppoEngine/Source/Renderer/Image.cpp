#include "pch.h"
#include "Image.h"

#include "Renderer/Allocator.h"
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

		static bool IsDepthFormat(ImageFormat format)
		{
			if (format == ImageFormat::Depth)
				return true;
			return false;
		}
	}

	Image::Image(const ImageSpecification& specification)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("Image::Image");

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

		m_Info.Allocation = Allocator::AllocateImage(m_Info.Image, imageInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Image View
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Info.Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = Utils::ImageFormatToVkFormat(m_Specification.Format);
		viewInfo.subresourceRange = {};
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_Info.ImageView), "Failed to create image view!");

		m_Info.ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	Image::~Image()
	{
		EPPO_PROFILE_FUNCTION("Image::~Image");

		Release();
	}

	void Image::Release()
	{
		EPPO_PROFILE_FUNCTION("Image::Release");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		if (m_Info.Sampler)
		{
			vkDestroySampler(device, m_Info.Sampler, nullptr);
			m_Info.Sampler = nullptr;
		}

		if (m_Info.ImageView)
		{
			vkDestroyImageView(device, m_Info.ImageView, nullptr);
			m_Info.ImageView = nullptr;
		}

		if (m_Info.Image)
		{
			Allocator::DestroyImage(m_Info.Image, m_Info.Allocation);
			m_Info.Image = nullptr;
			m_Info.Allocation = nullptr;
		}
	}
}
