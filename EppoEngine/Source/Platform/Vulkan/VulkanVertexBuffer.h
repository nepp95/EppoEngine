#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Renderer/Buffer/VertexBuffer.h"

namespace Eppo
{
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(void* data, uint32_t size);
		VulkanVertexBuffer(uint32_t size);
		~VulkanVertexBuffer();

		void SetData(void* data, uint32_t size) override;

		VkBuffer GetBuffer() const { return m_Buffer; }

	private:
		void CreateBuffer(VmaMemoryUsage usage, void* data);

	private:
		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
