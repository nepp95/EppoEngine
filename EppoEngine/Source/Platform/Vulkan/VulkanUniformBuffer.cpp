#include "pch.h"
#include "VulkanUniformBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	VulkanUniformBuffer::VulkanUniformBuffer(const uint32_t size, const uint32_t binding)
		: m_Size(size), m_Binding(binding)
	{
		m_Buffers.resize(VulkanConfig::MaxFramesInFlight);
		m_Allocations.resize(VulkanConfig::MaxFramesInFlight);
		m_MappedMemory.resize(VulkanConfig::MaxFramesInFlight);
		m_DescriptorBufferInfos.resize(VulkanConfig::MaxFramesInFlight);

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = m_Size;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			m_Allocations[i] = VulkanAllocator::AllocateBuffer(m_Buffers[i], bufferInfo, VMA_MEMORY_USAGE_CPU_ONLY);
			m_MappedMemory[i] = VulkanAllocator::MapMemory(m_Allocations[i]);

			m_DescriptorBufferInfos[i].buffer = m_Buffers[i];
			m_DescriptorBufferInfos[i].offset = 0;
			m_DescriptorBufferInfos[i].range = m_Size;
		}
	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			EPPO_MEM_WARN("Releasing uniform buffer {}", static_cast<void*>(this));
			VulkanAllocator::UnmapMemory(m_Allocations[i]);
			VulkanAllocator::DestroyBuffer(m_Buffers[i], m_Allocations[i]);
		}
	}

	void VulkanUniformBuffer::SetData(void* data, const uint32_t size)
	{
		EPPO_PROFILE_FUNCTION("VulkanUniformBuffer::SetData");
		EPPO_ASSERT(size == m_Size)

		const uint32_t imageIndex = VulkanContext::Get()->GetCurrentFrameIndex();
		memcpy(m_MappedMemory[imageIndex], data, size);
	}
}
