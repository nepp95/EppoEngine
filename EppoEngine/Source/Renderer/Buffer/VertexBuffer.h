#pragma once

#include "Core/Buffer.h"

namespace Eppo
{
	class VertexBuffer : public RefCounter
	{
	public:
		virtual ~VertexBuffer() {};

		virtual void SetData(void* data, uint32_t size) = 0;

		static Ref<VertexBuffer> Create(void* data, uint32_t size);
		static Ref<VertexBuffer> Create(uint32_t size);

	protected:
		uint32_t m_Size;
	};
}
