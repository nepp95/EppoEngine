#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Renderer/Buffer/UniformBuffer.h"

namespace Eppo
{
	class VulkanUniformBuffer : public UniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size, uint32_t binding);
		~VulkanUniformBuffer();

		void SetData(void* data, uint32_t size) override;

		const std::vector<VkBuffer>& GetBuffers() const { return m_Buffers; }
		const VkDescriptorBufferInfo& GetDescriptorBufferInfo() const { return m_DescriptorBufferInfo; }

	private:
		std::vector<VkBuffer> m_Buffers;
		std::vector<VmaAllocation> m_Allocations;
		std::vector<void*> m_MappedMemory;

		VkDescriptorBufferInfo m_DescriptorBufferInfo;
	};
}
