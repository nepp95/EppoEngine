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

	private:
		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
