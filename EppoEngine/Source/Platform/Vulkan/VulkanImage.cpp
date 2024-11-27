#include "pch.h"
#include "VulkanImage.h"

#include <stb_image.h>

namespace Eppo
{
	VulkanImage::VulkanImage(const ImageSpecification& specification)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("VulkanImage::VulkanImage");

		// Do we have data?
		if (!m_Specification.Filepath.empty())
		{
			int width;
			int height;
			int channels;

			stbi_set_flip_vertically_on_load(1);

			m_ImageData = stbi_load(m_Specification.Filepath.string().c_str(), &width, &height, &channels, 4);

			m_Specification.Width = width;
			m_Specification.Height = height;
			m_Specification.Format = ImageFormat::RGBA8;
			m_Specification.Usage = ImageUsage::Texture;
		}

		// Image usage
		VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
		if (m_Specification.Usage == ImageUsage::Texture)
		{
			usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		else
		{
			if (m_Specification.Format == ImageFormat::Depth)
				usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			else
				usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		// Image
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = m_Specification.Width;
		imageCreateInfo.extent.height = m_Specification.Height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = m_Specification.CubeMap ? 6 : 1;
		imageCreateInfo.format = Utils::ImageFormatToVkFormat(m_Specification.Format);
		imageCreateInfo.usage = usageFlags;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.flags = m_Specification.CubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		m_ImageInfo.Allocation = VulkanAllocator::AllocateImage(m_ImageInfo.Image, imageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Image view
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = m_ImageInfo.Image;
		imageViewCreateInfo.viewType = m_Specification.CubeMap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = Utils::ImageFormatToVkFormat(m_Specification.Format);
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = m_Specification.CubeMap ? 6 : 1;

		Ref<VulkanContext> context = VulkanContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
		VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_ImageInfo.ImageView), "Failed to create image view!");

		// Sampler
		Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();

		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = physicalDevice->GetDeviceProperties().limits.maxSamplerAnisotropy;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;

		VK_CHECK(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_ImageInfo.Sampler), "Failed to create sampler!");

		VkCommandBuffer commandBuffer = context->GetLogicalDevice()->GetCommandBuffer(true);
		TransitionImage(commandBuffer, m_ImageInfo.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		context->GetLogicalDevice()->FlushCommandBuffer(commandBuffer);

		if (m_Specification.Format == ImageFormat::Depth)
			m_ImageInfo.ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		else
			m_ImageInfo.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (m_ImageData)
		{
			SetData(m_ImageData);
			stbi_image_free(m_ImageData);
			m_ImageData = nullptr;
		}
	}

	VulkanImage::~VulkanImage()
	{
		Release();
	}

	void VulkanImage::SetData(void* data, uint32_t channels)
	{
		EPPO_PROFILE_FUNCTION("VulkanImage::SetData");

		uint64_t size = m_Specification.Width * m_Specification.Height * channels;

		// Create staging buffer
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = VulkanAllocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		void* memData = VulkanAllocator::MapMemory(stagingBufferAlloc);
		memcpy(memData, data, size);
		VulkanAllocator::UnmapMemory(stagingBufferAlloc);

		VkCommandBuffer commandBuffer = VulkanContext::Get()->GetLogicalDevice()->GetCommandBuffer(true);

		// Transition to layout optimal for transferring
		TransitionImage(commandBuffer, m_ImageInfo.Image, m_ImageInfo.ImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy image data to image
		VkBufferImageCopy copyRegion{};
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageExtent.width = m_Specification.Width;
		copyRegion.imageExtent.height = m_Specification.Height;
		copyRegion.imageExtent.depth = 1;
		copyRegion.bufferOffset = 0;

		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_ImageInfo.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Transition image back to presentable layout
		TransitionImage(commandBuffer, m_ImageInfo.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Flush command buffer
		VulkanContext::Get()->GetLogicalDevice()->FlushCommandBuffer(commandBuffer);

		VulkanAllocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
	}

	void VulkanImage::Release()
	{
		VkDevice device = VulkanContext::Get()->GetLogicalDevice()->GetNativeDevice();

		if (m_ImageInfo.Sampler)
		{
			EPPO_MEM_WARN("Releasing sampler {}", (void*)m_ImageInfo.Sampler);
			vkDestroySampler(device, m_ImageInfo.Sampler, nullptr);
			m_ImageInfo.Sampler = nullptr;
		}

		if (m_ImageInfo.ImageView)
		{
			EPPO_MEM_WARN("Releasing image view {}", (void*)m_ImageInfo.ImageView);
			vkDestroyImageView(device, m_ImageInfo.ImageView, nullptr);
			m_ImageInfo.ImageView = nullptr;
		}

		if (m_ImageInfo.Image)
		{
			EPPO_MEM_WARN("Releasing image {}", (void*)m_ImageInfo.Image);
			VulkanAllocator::DestroyImage(m_ImageInfo.Image, m_ImageInfo.Allocation);
			m_ImageInfo.Image = nullptr;
			m_ImageInfo.Allocation = nullptr;
		}
	}

	void VulkanImage::TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout srcLayout, VkImageLayout dstLayout)
	{
		VkImageMemoryBarrier2 imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
		imageBarrier.oldLayout = srcLayout;
		imageBarrier.newLayout = dstLayout;
		imageBarrier.image = image;
		imageBarrier.subresourceRange.aspectMask = GetImageAspectFlags(dstLayout);
		imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		imageBarrier.pNext = nullptr;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;
		depInfo.pNext = nullptr;

		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	VkImageAspectFlags VulkanImage::GetImageAspectFlags(VkImageLayout layout)
	{
		switch (layout)
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
			{
				return VK_IMAGE_ASPECT_NONE;
			}

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			{
				return VK_IMAGE_ASPECT_COLOR_BIT;
			}

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
			case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
			{
				return VK_IMAGE_ASPECT_DEPTH_BIT;
			}
		}

		EPPO_ASSERT(false);
		return VK_IMAGE_ASPECT_NONE;
	}
}
