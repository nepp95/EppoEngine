#include "pch.h"
#include "OpenGLRenderCommandBuffer.h"

#include "Renderer/Renderer.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLRenderCommandBuffer::OpenGLRenderCommandBuffer(uint32_t count)
	{
		m_Queries.push_back(OpenGLQuery(m_QueryIndex));

		m_Timestamps.resize(2);
		for (auto& timestamp : m_Timestamps)
			timestamp.resize(m_QueryCount);

		m_PipelineStatistics.resize(2);
	}

	OpenGLRenderCommandBuffer::~OpenGLRenderCommandBuffer()
	{}

	void OpenGLRenderCommandBuffer::Begin()
	{
		m_QueryIndex = 0;

		Renderer::SubmitCommand([this]()
		{
			m_Queries[m_QueryIndex].Begin();
		});
		
		// TODO: Pipeline statistics
	}

	void OpenGLRenderCommandBuffer::End()
	{
		Renderer::SubmitCommand([this]()
		{
			m_Queries[m_QueryIndex].End();
		});
	}

	void OpenGLRenderCommandBuffer::Submit()
	{
		Renderer::SubmitCommand([this]()
		{
			m_Timestamps[0][m_QueryIndex] = m_Queries.at(m_QueryIndex).GetQueryResults();
		});
	}

	uint32_t OpenGLRenderCommandBuffer::BeginTimestampQuery()
	{
		uint32_t queryIndex = m_QueryIndex;
		//m_QueryIndex++;

		//Renderer::SubmitCommand([this, queryIndex]()
		//	{
		//		glBeginQuery(GL_TIME_ELAPSED, queryIndex);
		//	});

		return queryIndex;
	}

	void OpenGLRenderCommandBuffer::EndTimestampQuery(uint32_t queryIndex)
	{
		//Renderer::SubmitCommand([this, queryIndex]()
		//{
		//	glEndQuery(GL_TIME_ELAPSED);
		//});
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
