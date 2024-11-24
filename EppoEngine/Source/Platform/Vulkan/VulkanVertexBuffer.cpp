#include "pch.h"
#include "VulkanVertexBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	VulkanVertexBuffer::VulkanVertexBuffer(Buffer buffer)
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

		// Create GPU local buffer
		VkBufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = buffer.Size;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		m_Allocation = VulkanAllocator::AllocateBuffer(m_Buffer, vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY);

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
}
