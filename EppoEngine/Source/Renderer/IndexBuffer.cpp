#include "pch.h"
#include "IndexBuffer.h"

#include <glad/glad.h>

namespace Eppo
{
	IndexBuffer::IndexBuffer(void* data, uint32_t size)
		: m_Size(size)
	{
		EPPO_PROFILE_FUNCTION("IndexBuffer::IndexBuffer");

		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, m_Size, data, GL_STATIC_DRAW);
	}

	IndexBuffer::~IndexBuffer()
	{
		EPPO_PROFILE_FUNCTION("IndexBuffer::~IndexBuffer");

		glDeleteBuffers(1, &m_RendererID);
	}

	void IndexBuffer::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}

	void IndexBuffer::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}
