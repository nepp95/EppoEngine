#include "pch.h"
#include "VulkanVertexBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	VulkanVertexBuffer::VulkanVertexBuffer(void* data, uint32_t size)
	{
		EPPO_PROFILE_FUNCTION("VulkanVertexBuffer::VulkanVertexBuffer");

		m_LocalStorage = Buffer::Copy(data, size);
		CreateBuffer(VMA_MEMORY_USAGE_GPU_ONLY);
	}

	VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size)
	{
		EPPO_PROFILE_FUNCTION("VulkanVertexBuffer::VulkanVertexBuffer");

		m_LocalStorage.Allocate(size);
		CreateBuffer(VMA_MEMORY_USAGE_CPU_TO_GPU);
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		EPPO_PROFILE_FUNCTION("VulkanVertexBuffer::~VulkanVertexBuffer");

		// TODO: Is this the memory leak :o
		m_LocalStorage.Release();
		Allocator::DestroyBuffer(m_Buffer, m_Allocation);
	}

	void VulkanVertexBuffer::SetData(void* data, uint32_t size)
	{
		EPPO_PROFILE_FUNCTION("VertexBuffer::SetData");
		EPPO_ASSERT((size <= m_LocalStorage.Size));
		// TODO: Should only be possible with a dynamic buffer, keep track of memory type?

		memcpy(m_LocalStorage.Data, (uint8_t*)data, size);

		void* memData = Allocator::MapMemory(m_Allocation);
		memcpy(memData, data, size);
		Allocator::UnmapMemory(m_Allocation);
	}

	void VulkanVertexBuffer::CreateBuffer(VmaMemoryUsage usage)
	{
		EPPO_PROFILE_FUNCTION("VertexBuffer::CreateBuffer");

		if (usage == VMA_MEMORY_USAGE_CPU_TO_GPU)
		{
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = m_LocalStorage.Size;
			bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

			m_Allocation = Allocator::AllocateBuffer(m_Buffer, bufferInfo, usage);
		}
		else if (usage == VMA_MEMORY_USAGE_GPU_ONLY)
		{
			// To stage or not to stage (staging buffer)
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = m_LocalStorage.Size;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkBuffer stagingBuffer;
			VmaAllocation stagingBufferAlloc = Allocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

			void* memData = Allocator::MapMemory(stagingBufferAlloc);
			memcpy(memData, m_LocalStorage.Data, m_LocalStorage.Size);
			Allocator::UnmapMemory(stagingBufferAlloc);

			// Device local buffer
			VkBufferCreateInfo vertexBufferInfo{};
			vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			vertexBufferInfo.size = m_LocalStorage.Size;
			vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

			m_Allocation = Allocator::AllocateBuffer(m_Buffer, vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

			// Copy staging buffer data to device local buffer
			Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
			Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();
			VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = m_LocalStorage.Size;

			vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
			logicalDevice->FlushCommandBuffer(commandBuffer);

			// Clean up
			Allocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
		}
	}
}
