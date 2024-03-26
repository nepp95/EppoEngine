#pragma once

namespace Eppo
{
	class VertexBuffer
	{
	public:
		VertexBuffer(void* data, uint32_t size);
		VertexBuffer(uint32_t size);
		~VertexBuffer();

		void Bind() const;
		void Unbind() const;

	private:
		uint32_t m_RendererID;
		uint32_t m_Size;
	};
}
