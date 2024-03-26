#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Renderer/Renderer.h"

#include <glad/glad.h>

namespace Eppo
{
	RenderCommandBuffer::RenderCommandBuffer(uint32_t count)
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::RenderCommandBuffer");

		glCreateQueries(GL_TIME_ELAPSED, 1, &m_QueryRendererID);
	}

	RenderCommandBuffer::~RenderCommandBuffer()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::~RenderCommandBuffer");
	}

	void RenderCommandBuffer::RT_Begin()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::Begin");

		Renderer::SubmitCommand([this]()
		{
			glBeginQuery(GL_TIME_ELAPSED, m_QueryRendererID);
		});
	}

	void RenderCommandBuffer::RT_End()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::End");

		Renderer::SubmitCommand([this]()
		{
			glEndQuery(GL_TIME_ELAPSED);
		});
	}

	void RenderCommandBuffer::RT_Submit()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::Submit");

		Renderer::SubmitCommand([this]()
		{
			// TODO: THIS WILL BLOCK THE THREAD!
			glGetQueryObjectui64v(m_QueryRendererID, GL_QUERY_RESULT, &m_Timestamp);
		});
	}
}
