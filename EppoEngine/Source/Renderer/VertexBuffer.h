#pragma once

#include "Core/Buffer.h"
#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() = default;

		virtual void SetData(Buffer buffer) = 0;

		static Ref<VertexBuffer> Create(uint32_t size);
		static Ref<VertexBuffer> Create(const void* data, uint32_t size);
		static Ref<VertexBuffer> Create(Buffer buffer);
	};
}
