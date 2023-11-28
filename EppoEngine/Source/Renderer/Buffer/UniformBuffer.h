#pragma once

#include "Renderer/Allocator.h"
#include "Renderer/Shader.h"

namespace Eppo
{
	class UniformBuffer
	{
	public:
		UniformBuffer(const Ref<Shader>& shader, uint32_t size);
		~UniformBuffer();
		UniformBuffer(const UniformBuffer&) = delete;
		UniformBuffer& operator=(const UniformBuffer&) = delete;

		void SetData(void* data, uint32_t size);

		const std::vector<VkBuffer>& GetBuffers() const { return m_Buffers; }
		const std::vector<VkDescriptorBufferInfo>& GetDescriptorBufferInfos() const { return m_DescriptorBufferInfos; }
		VkDescriptorSet GetDescriptorSet(uint32_t imageIndex);
		VkDescriptorSet GetCurrentDescriptorSet();

	private:
		uint32_t m_Size;

		std::vector<VkBuffer> m_Buffers;
		std::vector<VmaAllocation> m_Allocations;
		std::vector<void*> m_MappedMemory;

		std::vector<VkDescriptorBufferInfo> m_DescriptorBufferInfos;
		std::vector<VkDescriptorSet> m_DescriptorSets;
	};
}
