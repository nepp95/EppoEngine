#include "pch.h"
#include "OpenGLUniformBuffer.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t binding)
		: UniformBuffer(size, binding)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_Binding, m_RendererID);
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLUniformBuffer::SetData(void* data, uint32_t size)
	{
		EPPO_ASSERT(size == m_Size);

		glNamedBufferSubData(m_RendererID, 0, size, data);
	}
}
