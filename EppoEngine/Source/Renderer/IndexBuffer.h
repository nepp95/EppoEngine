#pragma once

#include "Renderer/Allocator.h"

namespace Eppo
{
	class IndexBuffer
	{
	public:
		IndexBuffer(void* data, uint32_t size);
		~IndexBuffer() = default;

		VkBuffer GetBuffer() const { return m_Buffer; }

		uint32_t GetIndexCount() const { return m_Size / sizeof(uint32_t); }

	private:
		uint32_t m_Size;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
