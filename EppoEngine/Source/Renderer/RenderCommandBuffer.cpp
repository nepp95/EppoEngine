#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Renderer/Renderer.h"

#include <glad/glad.h>

namespace Eppo
{
	RenderCommandBuffer::RenderCommandBuffer(uint32_t count)
	{
		glCreateQueries(GL_TIME_ELAPSED, 1, &m_QueryRendererID);
	}

	void RenderCommandBuffer::RT_Begin()
	{
		Renderer::SubmitCommand([this]()
		{
			glBeginQuery(GL_TIME_ELAPSED, m_QueryRendererID);
		});
	}

	void RenderCommandBuffer::RT_End()
	{
		Renderer::SubmitCommand([this]()
		{
			glEndQuery(GL_TIME_ELAPSED);
		});
	}

	void RenderCommandBuffer::RT_Submit()
	{
		Renderer::SubmitCommand([this]()
		{
			// TODO: THIS WILL BLOCK THE THREAD!
			glGetQueryObjectui64v(m_QueryRendererID, GL_QUERY_RESULT, &m_Timestamp);
		});
	}
}
