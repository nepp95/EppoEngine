#include "pch.h"
#include "UniformBuffer.h"

#include "Renderer/Renderer.h"

#include <glad/glad.h>

namespace Eppo
{
	UniformBuffer::UniformBuffer(uint32_t size, uint32_t binding)
		: m_Size(size), m_Binding(binding)
	{
		EPPO_PROFILE_FUNCTION("UniformBuffer::UniformBuffer");

		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, m_Size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_Binding, m_RendererID);
	}

	UniformBuffer::~UniformBuffer()
	{
		EPPO_PROFILE_FUNCTION("UniformBuffer::~UniformBuffer");

		glDeleteBuffers(1, &m_RendererID);
	}

	void UniformBuffer::SetData(void* data, uint32_t size)
	{
		Renderer::SubmitCommand([this, size, data](){
			EPPO_PROFILE_FUNCTION("UniformBuffer::SetData");
			EPPO_ASSERT(size == m_Size);

			glNamedBufferSubData(m_RendererID, 0, size, data);
		});
	}
}
