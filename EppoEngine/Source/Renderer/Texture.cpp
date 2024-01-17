#include "pch.h"
#include "Texture.h"

#include "Renderer/Allocator.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <stb_image.h>

namespace Eppo
{
	Texture::Texture(const std::filesystem::path& filepath)
		: m_Filepath(filepath)
	{
		EPPO_PROFILE_FUNCTION("Texture::Texture");

		// Read pixels
		int width, height, channels;
		stbi_uc* data = nullptr;

		m_ImageData.Data = stbi_load(filepath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		m_ImageData.Size = width * height * channels;
		m_Width = width;
		m_Height = height;

		EPPO_ASSERT(m_ImageData);

		ImageSpecification imageSpec;
		imageSpec.Width = m_Width;
		imageSpec.Height = m_Height;
		imageSpec.Format = ImageFormat::RGBA8;
		imageSpec.Usage = ImageUsage::Texture;

		m_Image = CreateRef<Image>(imageSpec);

		if (m_ImageData)
		{
			VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

			// Create a staging buffer for copy
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = m_ImageData.Size;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkBuffer stagingBuffer;
			VmaAllocation stagingBufferAlloc = VulkanAllocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

			// Map staging buffer memory
			void* memData = VulkanAllocator::MapMemory(stagingBufferAlloc);
			memcpy(memData, m_ImageData.Data, m_ImageData.Size);
			VulkanAllocator::UnmapMemory(stagingBufferAlloc);

			ImageInfo& info = m_Image->GetImageInfo();

			// Image transition for data copy
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = info.Image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			
			Ref<VulkanLogicalDevice> logicalDevice = RendererContext::Get()->GetLogicalDevice();
			VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			// Data copy
			VkBufferImageCopy copyRegion{};
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageExtent.width = m_Width;
			copyRegion.imageExtent.height = m_Height;
			copyRegion.imageExtent.depth = 1;
			copyRegion.bufferOffset = 0;

			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, info.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			// Image transition for shader read
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			logicalDevice->FlushCommandBuffer(commandBuffer);

			// Update image layout in descriptor image info
			info.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Clean up
			VulkanAllocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
			stbi_image_free(m_ImageData.Data);
			m_ImageData = Buffer();
		}
	}

	Texture::Texture(uint32_t width, uint32_t height, ImageFormat format, void* data)
		: m_Width(width), m_Height(height)
	{
		EPPO_PROFILE_FUNCTION("Texture::Texture");

		uint32_t size = width * height * Utils::GetMemorySize(format);

		if (data)
		{
			m_ImageData.Data = (uint8_t*)data;
			m_ImageData.Size = size;
		}

		ImageSpecification imageSpec;
		imageSpec.Width = m_Width;
		imageSpec.Height = m_Height;
		imageSpec.Format = ImageFormat::RGBA8;
		imageSpec.Usage = ImageUsage::Texture;

		m_Image = CreateRef<Image>(imageSpec);

		if (m_ImageData)
		{
			VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

			// Create a staging buffer for copy
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = m_ImageData.Size;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkBuffer stagingBuffer;
			VmaAllocation stagingBufferAlloc = VulkanAllocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

			// Map staging buffer memory
			void* memData = VulkanAllocator::MapMemory(stagingBufferAlloc);
			memcpy(memData, m_ImageData.Data, m_ImageData.Size);
			VulkanAllocator::UnmapMemory(stagingBufferAlloc);

			ImageInfo& info = m_Image->GetImageInfo();

			// Image transition for data copy
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = info.Image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_NONE;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			Ref<VulkanLogicalDevice> logicalDevice = RendererContext::Get()->GetLogicalDevice();
			VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			// Data copy
			VkBufferImageCopy copyRegion{};
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageExtent.width = m_Width;
			copyRegion.imageExtent.height = m_Height;
			copyRegion.imageExtent.depth = 1;
			copyRegion.bufferOffset = 0;

			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, info.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			// Image transition for shader read
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			logicalDevice->FlushCommandBuffer(commandBuffer);

			// Create sampler
			Ref<VulkanPhysicalDevice> physicalDevice = RendererContext::Get()->GetPhysicalDevice();

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

			VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &info.Sampler), "Failed to create sampler!");

			// Update image layout in descriptor image info
			info.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Clean up
			VulkanAllocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
		}
	}

	Texture::~Texture()
	{
		EPPO_PROFILE_FUNCTION("Texture::~Texture");

		m_Image->Release();
	}
}
