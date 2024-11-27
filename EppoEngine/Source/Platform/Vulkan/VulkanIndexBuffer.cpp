#include "pch.h"
#include "VulkanIndexBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	VulkanIndexBuffer::VulkanIndexBuffer(void* data, uint32_t size)
		: m_Size(size)
	{
		EPPO_PROFILE_FUNCTION("VulkanIndexBuffer::VulkanIndexBuffer");

		// Create staging buffer
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = m_Size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = VulkanAllocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// Copy data to staging buffer
		void* memData = VulkanAllocator::MapMemory(stagingBufferAlloc);
		memcpy(memData, data, m_Size);
		VulkanAllocator::UnmapMemory(stagingBufferAlloc);

		// Create GPU local buffer
		VkBufferCreateInfo indexBufferInfo{};
		indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferInfo.size = m_Size;
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, indexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Copy data from staging buffer to GPU local buffer
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = m_Size;

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		// Clean up
		VulkanAllocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		EPPO_MEM_WARN("Releasing index buffer {}", (void*)this);
		VulkanAllocator::DestroyBuffer(m_Buffer, m_Allocation);
	}
}
