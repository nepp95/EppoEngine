#pragma once

namespace Eppo
{
	class IndexBuffer
	{
	public:
		IndexBuffer(void* data, uint32_t size);
		~IndexBuffer();

		void Bind() const;
		void Unbind() const;

		uint32_t GetIndexCount() const { return m_Size / sizeof(uint32_t); }

	private:
		uint32_t m_RendererID;
		uint32_t m_Size;
	};
}
