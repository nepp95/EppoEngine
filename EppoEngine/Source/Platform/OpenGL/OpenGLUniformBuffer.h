#pragma once

#include "Renderer/Buffer/UniformBuffer.h"

namespace Eppo
{
	class OpenGLUniformBuffer : public UniformBuffer
	{
	public:
		OpenGLUniformBuffer(uint32_t size, uint32_t binding);
		virtual ~OpenGLUniformBuffer();

		void SetData(void* data, uint32_t size) override;
		
	private:
		uint32_t m_RendererID;
	};
}
