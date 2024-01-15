#pragma once

namespace Eppo
{
	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() {};

		virtual void SetData(void* data, uint32_t size) = 0;

		static Ref<VertexBuffer> Create(void* data, uint32_t size);
		static Ref<VertexBuffer> Create(uint32_t size);
	};
}
