#include "pch.h"
#include "Image.h"

#include "Renderer/Allocator.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Image::Image(const ImageSpecification& specification)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("Image::Image");

		VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;

		if (m_Specification.Usage == ImageUsage::Texture)
			usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
		viewInfo.subresourceRange.aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_Info.ImageView), "Failed to create image view!");

		// Create sampler
		Ref<PhysicalDevice> physicalDevice = RendererContext::Get()->GetPhysicalDevice();

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = physicalDevice->GetDeviceProperties().limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &m_Info.Sampler), "Failed to create sampler!");

		m_Info.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (m_Specification.Format == ImageFormat::Depth)
			m_DescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		else
			m_DescriptorImageInfo.imageLayout = m_Info.ImageLayout;
		m_DescriptorImageInfo.imageView = m_Info.ImageView;
		m_DescriptorImageInfo.sampler = m_Info.Sampler;
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
