#pragma once

#include "Renderer/Buffer/IndexBuffer.h"

namespace Eppo
{
	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer(void* data, uint32_t size);
		virtual ~OpenGLIndexBuffer();

		void Bind() const;
		void Unbind() const;

	private:
		uint32_t m_RendererID;
	};
}
