#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/IndexBuffer.h"

namespace Eppo
{
	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(void* data, uint32_t size);
		virtual ~VulkanIndexBuffer();

		VkBuffer GetBuffer() const { return m_Buffer; }
		void RT_Bind(Ref<CommandBuffer> commandBuffer) const;

		uint32_t GetIndexCount() const override { return m_Size / sizeof(uint32_t); }

	private:
		uint32_t m_Size;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
