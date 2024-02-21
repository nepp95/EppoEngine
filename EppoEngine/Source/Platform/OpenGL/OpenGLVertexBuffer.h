#pragma once

#include "Renderer/Buffer/VertexBuffer.h"

namespace Eppo
{
	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer(void* data, uint32_t size);
		OpenGLVertexBuffer(uint32_t size);
		virtual ~OpenGLVertexBuffer();

		void SetData(void* data, uint32_t size) override;

		void Bind() const;
		void Unbind() const;

	private:
		uint32_t m_RendererID;
	};
}
