#pragma once

#include "Core/Buffer.h"
#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() = default;

		virtual void RT_Bind(Ref<CommandBuffer> commandBuffer) const = 0;

		static Ref<VertexBuffer> Create(void* data, uint32_t size);
		static Ref<VertexBuffer> Create(Buffer buffer);
	};
}
