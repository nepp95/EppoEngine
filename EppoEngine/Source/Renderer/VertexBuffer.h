#pragma once

#include "Core/Buffer.h"
#include "Renderer/Allocator.h"
#include "Renderer/RenderCommandBuffer.h"

namespace Eppo
{
	class VertexBuffer
	{
	public:
		VertexBuffer(void* data, uint32_t size);
		~VertexBuffer();

		VkBuffer GetBuffer() const { return m_Buffer; }
		void RT_Bind(Ref<RenderCommandBuffer> renderCommandBuffer) const;

	private:
		void CreateBuffer(VmaMemoryUsage usage);

	private:
		Buffer m_LocalStorage;

		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};
}
