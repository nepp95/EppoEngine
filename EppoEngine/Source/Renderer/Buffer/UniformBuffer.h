#pragma once

#include "Renderer/Allocator.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	class UniformBuffer
	{
	public:
		UniformBuffer(uint32_t size);
		~UniformBuffer();
		UniformBuffer(const UniformBuffer&) = delete;
		UniformBuffer& operator=(const UniformBuffer&) = delete;

		void SetData(void* data, uint32_t size);

		const std::vector<VkBuffer>& GetBuffers() const { return m_Buffers; }
		const VkDescriptorBufferInfo& GetDescriptorBufferInfo() const { return m_DescriptorBufferInfo; }
		VkDescriptorSet GetDescriptorSet(uint32_t imageIndex);
		VkDescriptorSet GetCurrentDescriptorSet();

	private:
		uint32_t m_Size;

		std::vector<VkBuffer> m_Buffers;
		std::vector<VmaAllocation> m_Allocations;
		std::vector<void*> m_MappedMemory;

		VkDescriptorBufferInfo m_DescriptorBufferInfo;
		std::vector<VkDescriptorSet> m_DescriptorSets;
	};
}
