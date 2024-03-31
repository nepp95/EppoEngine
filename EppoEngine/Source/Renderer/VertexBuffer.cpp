#include "pch.h"
#include "VertexBuffer.h"

#include <glad/glad.h>

namespace Eppo
{
	VertexBuffer::VertexBuffer(void* data, uint32_t size)
		: m_Size(size)
	{
		EPPO_PROFILE_FUNCTION("VertexBuffer::VertexBuffer");

		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, m_Size, data, GL_STATIC_DRAW);
	}

	VertexBuffer::VertexBuffer(uint32_t size)
		: m_Size(size)
	{
		EPPO_PROFILE_FUNCTION("VertexBuffer::VertexBuffer");

		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, m_Size, nullptr, GL_DYNAMIC_DRAW);
	}

	VertexBuffer::~VertexBuffer()
	{
		EPPO_PROFILE_FUNCTION("VertexBuffer::~VertexBuffer");

		glDeleteBuffers(1, &m_RendererID);
	}

	void VertexBuffer::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}

	void VertexBuffer::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}
