#pragma once

#include "Core/Buffer.h"
#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() = default;

		virtual void SetData(Buffer buffer) = 0;
		virtual uint32_t GetIndexCount() const = 0;

		static Ref<IndexBuffer> Create(uint32_t size);
		static Ref<IndexBuffer> Create(void* data, uint32_t size);
		static Ref<IndexBuffer> Create(Buffer buffer);
	};
}
