#pragma once

#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {};

		virtual void RT_Bind(Ref<CommandBuffer> commandBuffer) const = 0;
		virtual uint32_t GetIndexCount() const = 0;

		static Ref<IndexBuffer> Create(void* data, uint32_t size);
	};
}
