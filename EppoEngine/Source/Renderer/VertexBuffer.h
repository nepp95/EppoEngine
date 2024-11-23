#pragma once

#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() {}

		virtual void RT_Bind(Ref<CommandBuffer> commandBuffer) const = 0;

		static Ref<VertexBuffer> Create(void* data, uint32_t size);
	};
}
