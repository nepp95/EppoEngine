#include "pch.h"
#include "UniformBuffer.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	UniformBuffer::UniformBuffer(uint32_t size)
		: m_Size(size)
	{
		m_Allocations.resize(VulkanConfig::MaxFramesInFlight);
		m_Buffers.resize(VulkanConfig::MaxFramesInFlight);
		m_MappedMemory.resize(VulkanConfig::MaxFramesInFlight);

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = m_Size;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			m_Allocations[i] = Allocator::AllocateBuffer(m_Buffers[i], bufferInfo, VMA_MEMORY_USAGE_CPU_COPY);
			m_MappedMemory[i] = Allocator::MapMemory(m_Allocations[i]);

			VkDescriptorBufferInfo& descriptorBufferInfo = m_DescriptorBufferInfos.emplace_back();
			descriptorBufferInfo.buffer = m_Buffers[i];
			descriptorBufferInfo.offset = 0;
			descriptorBufferInfo.range = m_Size;
		}
	}

	UniformBuffer::~UniformBuffer()
	{
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			Allocator::UnmapMemory(m_Allocations[i]);
			Allocator::DestroyBuffer(m_Buffers[i], m_Allocations[i]);
		}
	}

	void UniformBuffer::SetData(void* data, uint32_t size)
	{
		EPPO_ASSERT(size == m_Size);

		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		memcpy(m_MappedMemory[imageIndex], data, size);
	}

	VkDescriptorSet UniformBuffer::GetCurrentDescriptorSet()
	{
		Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
		uint32_t imageIndex = swapchain->GetCurrentImageIndex();

		return m_DescriptorSets[imageIndex];
	}
}
