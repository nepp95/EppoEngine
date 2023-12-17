#include "pch.h"
#include "UniformBuffer.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	UniformBuffer::UniformBuffer(uint32_t size, uint32_t binding)
		: m_Size(size), m_Binding(binding)
	{
		EPPO_PROFILE_FUNCTION("UniformBuffer::UniformBuffer");

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
			m_Allocations[i] = Allocator::AllocateBuffer(m_Buffers[i], bufferInfo, VMA_MEMORY_USAGE_CPU_ONLY);
			m_MappedMemory[i] = Allocator::MapMemory(m_Allocations[i]);

			m_DescriptorBufferInfo.buffer = m_Buffers[i];
			m_DescriptorBufferInfo.offset = 0;
			m_DescriptorBufferInfo.range = m_Size;
		}
	}

	UniformBuffer::~UniformBuffer()
	{
		EPPO_PROFILE_FUNCTION("UniformBuffer::~UniformBuffer");

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			Allocator::UnmapMemory(m_Allocations[i]);
			Allocator::DestroyBuffer(m_Buffers[i], m_Allocations[i]);
		}
	}

	void UniformBuffer::SetData(void* data, uint32_t size)
	{
		EPPO_PROFILE_FUNCTION("UniformBuffer::SetData");
		EPPO_ASSERT(size == m_Size);

		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

		memcpy(m_MappedMemory[imageIndex], data, size);
	}
}
