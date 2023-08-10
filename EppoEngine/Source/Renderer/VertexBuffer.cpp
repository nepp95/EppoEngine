#include "pch.h"
#include "VertexBuffer.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	VertexBuffer::VertexBuffer(void* data, uint32_t size)
		: m_Size(size), m_Offset(size)
	{
		CreateBuffer(data, m_Size);
	}

	VertexBuffer::VertexBuffer(uint32_t size)
		: m_Size(size), m_Offset(0)
	{
		CreateBuffer(nullptr, m_Size);
	}

	VertexBuffer::~VertexBuffer()
	{
		Allocator::DestroyBuffer(m_Buffer, m_Allocation);
	}

	void VertexBuffer::AddData(void* data, uint32_t size)
	{
		EPPO_ASSERT((m_Offset + size < m_Size));
		EPPO_ASSERT((size <= 65536)); // Hard limit of vkCmdUpdateBuffer

		Ref<LogicalDevice> logicalDevice = RendererContext::Get()->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		vkCmdUpdateBuffer(commandBuffer, m_Buffer, m_Offset, size, data);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		m_Offset += size;
	}

	void VertexBuffer::Reset()
	{
		if (m_Offset == 0)
			return;

		// Create a staging buffer for copy
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = m_Offset; // We only need to reset the part of the buffer that got changed
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = Allocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// Map staging buffer memory
		void* memData = Allocator::MapMemory(stagingBufferAlloc);
		memset(memData, 0, m_Offset);
		Allocator::UnmapMemory(stagingBufferAlloc);

		// Get a command buffer to execute a copy command
		auto& logicalDevice = RendererContext::Get()->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		// Do the copy to our GPU only memory
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = m_Offset;

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		// Clean up
		Allocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
		m_Offset = 0;
	}

	void VertexBuffer::CreateBuffer(void* data, uint32_t size)
	{
		// To stage or not to stage (staging buffer)
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = m_Size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = Allocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		void* memData = Allocator::MapMemory(stagingBufferAlloc);

		if (data)
			memcpy(memData, data, m_Size);
		else
			memset(memData, 0, m_Size);
		
		Allocator::UnmapMemory(stagingBufferAlloc);

		// Device local buffer
		VkBufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = m_Size;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_Allocation = Allocator::AllocateBuffer(m_Buffer, vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Copy staging buffer data to device local buffer
		Ref<LogicalDevice> logicalDevice = RendererContext::Get()->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = m_Size;

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		// Clean up
		Allocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
	}
}
