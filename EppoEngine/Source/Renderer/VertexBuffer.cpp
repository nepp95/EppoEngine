#include "pch.h"
#include "VertexBuffer.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	VertexBuffer::VertexBuffer(void* data, uint32_t size)
	{
		EPPO_PROFILE_FUNCTION("VertexBuffer::VertexBuffer");

		m_LocalStorage = Buffer::Copy(data, size);
		CreateBuffer(VMA_MEMORY_USAGE_GPU_ONLY);
	}

	void VertexBuffer::CreateBuffer(VmaMemoryUsage usage)
	{
		// Create staging buffer
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = m_LocalStorage.Size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = Allocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// Copy data to staging buffer
		void* memData = Allocator::MapMemory(stagingBufferAlloc);
		memcpy(memData, m_LocalStorage.Data, m_LocalStorage.Size);
		Allocator::UnmapMemory(stagingBufferAlloc);

		// Create GPU local buffer
		VkBufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = m_LocalStorage.Size;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		m_Allocation = Allocator::AllocateBuffer(m_Buffer, vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Copy data from staging buffer to GPU local buffer
		Ref<LogicalDevice> logicalDevice = RendererContext::Get()->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = m_LocalStorage.Size;

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		// Clean up
		Allocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);

		RendererContext::Get()->SubmitResourceFree([=]()
		{
			m_LocalStorage.Release();
			Allocator::DestroyBuffer(m_Buffer, m_Allocation);
		});
	}
}
