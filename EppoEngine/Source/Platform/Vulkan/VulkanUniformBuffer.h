#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Renderer/UniformBuffer.h"

namespace Eppo
{
	class VulkanUniformBuffer : public UniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size, uint32_t binding);
		~VulkanUniformBuffer();

		void SetData(void* data, uint32_t size) override;

		const std::vector<VkBuffer>& GetBuffers() const { return m_Buffers; }
		uint32_t GetBinding() const override { return m_Binding; }

		const std::vector<VkDescriptorBufferInfo>& GetDescriptorBufferInfos() const { return m_DescriptorBufferInfos; }

	private:
		uint32_t m_Size;
		uint32_t m_Binding;

		std::vector<VkBuffer> m_Buffers;
		std::vector<VmaAllocation> m_Allocations;
		std::vector<void*> m_MappedMemory;

		std::vector<VkDescriptorBufferInfo> m_DescriptorBufferInfos;
	};
}
