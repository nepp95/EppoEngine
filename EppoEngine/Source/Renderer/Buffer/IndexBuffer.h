#pragma once

namespace Eppo
{
	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {}

		virtual uint32_t GetIndexCount() const = 0;

		static Ref<IndexBuffer> Create(void* data, uint32_t size);
	};
}
