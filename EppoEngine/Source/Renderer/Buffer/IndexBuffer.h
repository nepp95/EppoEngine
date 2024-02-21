#pragma once

namespace Eppo
{
	class IndexBuffer : public RefCounter
	{
	public:
		virtual ~IndexBuffer() {}

		uint32_t GetIndexCount() const { return m_Size / sizeof(uint32_t); }

		static Ref<IndexBuffer> Create(void* data, uint32_t size);

	protected:
		uint32_t m_Size;
	};
}
