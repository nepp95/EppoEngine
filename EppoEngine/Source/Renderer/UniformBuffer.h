#pragma once

#include "Renderer/Allocator.h"

namespace Eppo
{
	class UniformBuffer
	{
	public:
		UniformBuffer(uint32_t size, uint32_t binding);
		~UniformBuffer();

		void SetData(void* data, uint32_t size);

		const std::vector<VkBuffer>& GetBuffers() const { return m_Buffers; }
		uint32_t GetBinding() const { return m_Binding; }

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
