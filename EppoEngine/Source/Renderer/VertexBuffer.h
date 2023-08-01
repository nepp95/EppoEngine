#pragma once

#include "Renderer/Allocator.h"

namespace Eppo
{
	class VertexBuffer
	{
	public:
		VertexBuffer(void* data, uint32_t size);
		VertexBuffer(uint32_t size);
		~VertexBuffer();

		void AddData(void* data, uint32_t size);
		void Reset();

		VkBuffer GetBuffer() const { return m_Buffer; }

	private:
		void CreateBuffer(void* data, uint32_t size);

	private:
		uint32_t m_Size;
		uint32_t m_Offset;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
