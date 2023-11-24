#pragma once

#include "Core/Buffer.h"
#include "Renderer/Allocator.h"

namespace Eppo
{
	class VertexBuffer
	{
	public:
		VertexBuffer(void* data, uint32_t size);
		VertexBuffer(uint32_t size);
		~VertexBuffer();
		VertexBuffer(const VertexBuffer&) = delete;
		VertexBuffer& operator=(const VertexBuffer&) = delete;

		void SetData(void* data, uint32_t size);

		VkBuffer GetBuffer() const { return m_Buffer; }

	private:
		void CreateBuffer(VmaMemoryUsage usage);

	private:
		Buffer m_LocalStorage;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
