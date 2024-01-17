#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Renderer/Buffer/IndexBuffer.h"

namespace Eppo
{
	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(void* data, uint32_t size);
		~VulkanIndexBuffer();

		VkBuffer GetBuffer() const { return m_Buffer; }

		uint32_t GetIndexCount() const override;

	private:
		uint32_t m_Size;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
