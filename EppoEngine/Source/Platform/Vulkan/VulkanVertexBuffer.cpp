#include "pch.h"
#include "VulkanVertexBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	
	VulkanVertexBuffer::VulkanVertexBuffer(void* data, uint32_t size)
	{
		m_LocalStorage = Buffer::Copy(data, size);
		CreateBuffer(VMA_MEMORY_USAGE_GPU_ONLY);
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		EPPO_WARN("Releasing vertex buffer {}", (void*)this);
		VulkanAllocator::DestroyBuffer(m_Buffer, m_Allocation);
	}

	void VulkanVertexBuffer::RT_Bind(Ref<CommandBuffer> commandBuffer) const
	{
		Renderer::SubmitCommand([this, commandBuffer]()
		{
			auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);

			VkBuffer vb = { m_Buffer };
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindVertexBuffers(cmd->GetCurrentCommandBuffer(), 0, 1, &vb, offsets);
		});
	}

	void VulkanVertexBuffer::CreateBuffer(VmaMemoryUsage usage)
	{
		// Create staging buffer
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = m_LocalStorage.Size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = VulkanAllocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		// Copy data to staging buffer
		void* memData = VulkanAllocator::MapMemory(stagingBufferAlloc);
		memcpy(memData, m_LocalStorage.Data, m_LocalStorage.Size);
		VulkanAllocator::UnmapMemory(stagingBufferAlloc);

		// Create GPU local buffer
		VkBufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = m_LocalStorage.Size;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

		// Copy data from staging buffer to GPU local buffer
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();
		VkCommandBuffer commandBuffer = logicalDevice->GetCommandBuffer(true);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = m_LocalStorage.Size;

		vkCmdCopyBuffer(commandBuffer, stagingBuffer, m_Buffer, 1, &copyRegion);
		logicalDevice->FlushCommandBuffer(commandBuffer);

		// Clean up
		VulkanAllocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
		m_LocalStorage.Release();
	}
}
