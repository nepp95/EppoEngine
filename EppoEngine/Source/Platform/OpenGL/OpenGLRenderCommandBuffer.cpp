#include "pch.h"
#include "OpenGLRenderCommandBuffer.h"

#include "Renderer/Renderer.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLRenderCommandBuffer::OpenGLRenderCommandBuffer(uint32_t count)
	{
		m_Timestamps.resize(2);
		for (auto& timestamp : m_Timestamps)
			timestamp.resize(m_QueryCount);

		m_PipelineStatistics.resize(2);
	}

	OpenGLRenderCommandBuffer::~OpenGLRenderCommandBuffer()
	{

	}

	void OpenGLRenderCommandBuffer::Begin()
	{
		m_QueryIndex = 1;

		Renderer::SubmitCommand([this]()
		{
			glBeginQuery(GL_TIME_ELAPSED, 0);
		});
		
		// TODO: Pipeline statistics
	}

	void OpenGLRenderCommandBuffer::End()
	{
		Renderer::SubmitCommand([this]()
		{
			glEndQuery(GL_TIME_ELAPSED);
		});
	}

	void OpenGLRenderCommandBuffer::Submit()
	{
		Renderer::SubmitCommand([this]()
		{
			// TODO: Multiple image indexes
			glGetQueryObjectui64v(0, GL_QUERY_RESULT, &m_Timestamps[0][0]);

		});
	}

	uint32_t OpenGLRenderCommandBuffer::BeginTimestampQuery()
	{
		uint32_t queryIndex = m_QueryIndex;
		m_QueryIndex++;

		Renderer::SubmitCommand([this, queryIndex]()
		{
			glBeginQuery(GL_TIME_ELAPSED, queryIndex);
		});

		return queryIndex;
	}

	void OpenGLRenderCommandBuffer::EndTimestampQuery(uint32_t queryIndex)
	{
		Renderer::SubmitCommand([this, queryIndex]()
		{
			glEndQuery(GL_TIME_ELAPSED);
		});
	}

	float OpenGLRenderCommandBuffer::GetTimestamp(uint32_t imageIndex, uint32_t queryIndex) const
	{
		return m_Timestamps[imageIndex][queryIndex];
	}

	const PipelineStatistics& OpenGLRenderCommandBuffer::GetPipelineStatistics(uint32_t imageIndex) const
	{
		return m_PipelineStatistics[imageIndex];
	}
}
