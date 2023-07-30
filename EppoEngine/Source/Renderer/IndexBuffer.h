#pragma once

#include "Renderer/Allocator.h"

namespace Eppo
{
	class IndexBuffer
	{
	public:
		IndexBuffer(void* data, uint32_t size);
		~IndexBuffer();

		VkBuffer GetBuffer() const { return m_Buffer; }

	private:
		uint32_t m_Size;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
