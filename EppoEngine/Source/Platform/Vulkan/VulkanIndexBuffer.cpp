#include "pch.h"
#include "VulkanIndexBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t size)
		: m_Size(size), m_IsMemoryMapped(true)
	{
		// Create GPU local buffer
		VkBufferCreateInfo indexBufferInfo{};
		indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferInfo.size = m_Size;
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, indexBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);
		m_MappedMemory = VulkanAllocator::MapMemory(m_Allocation);
	}

	VulkanIndexBuffer::VulkanIndexBuffer(Buffer buffer)
		: m_Size(buffer.Size), m_IsMemoryMapped(false)
	{
		EPPO_PROFILE_FUNCTION("VulkanIndexBuffer::VulkanIndexBuffer");

		// Create GPU local buffer
		VkBufferCreateInfo indexBufferInfo{};
		indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferInfo.size = m_Size;
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, indexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		CopyWithStagingBuffer(buffer);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		EPPO_MEM_WARN("Releasing index buffer {}", (void*)this);

		if (m_IsMemoryMapped)
			VulkanAllocator::UnmapMemory(m_Allocation);
		VulkanAllocator::DestroyBuffer(m_Buffer, m_Allocation);
	}

	void VulkanIndexBuffer::SetData(Buffer buffer)
	{
		// If buffer is bigger than our GPU buffer, recreate buffer
		if (buffer.Size > m_Size)
		{
			// Create GPU local buffer
			VkBufferCreateInfo indexBufferInfo{};
			indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexBufferInfo.size = m_Size;
			indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (m_IsMemoryMapped)
			{
				VulkanAllocator::UnmapMemory(m_Allocation);
				m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, indexBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);
				m_MappedMemory = VulkanAllocator::MapMemory(m_Allocation);
			} else
			{
				m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, indexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);
			}

			m_Size = buffer.Size;
		}

		// Now we have a GPU buffer of the correct size, copy data using either staging buffer or mapped memory
		if (m_IsMemoryMapped)
			memcpy(m_MappedMemory, buffer.Data, buffer.Size);
		else
			CopyWithStagingBuffer(buffer);
	}

	void VulkanIndexBuffer::CopyWithStagingBuffer(Buffer buffer) const
	{
		// Create staging buffer
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = buffer.Size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = VulkanAllocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// Copy data to staging buffer
		void* memData = VulkanAllocator::MapMemory(stagingBufferAlloc);
		memcpy(memData, buffer.Data, buffer.Size);
		VulkanAllocator::UnmapMemory(stagingBufferAlloc);

		// Copy data from staging buffer to GPU local buffer
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = buffer.Size;

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		// Clean up
		VulkanAllocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
	}
}
