#include "pch.h"
#include "OpenGLQuery.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLQuery::OpenGLQuery(uint32_t queryIndex)
		: m_QueryIndex(queryIndex)
	{
		glCreateQueries(GL_TIME_ELAPSED, 1, &m_RendererID);
	}

	OpenGLQuery::~OpenGLQuery()
	{
		glDeleteQueries(1, &m_RendererID);
	}

	void OpenGLQuery::Begin() const
	{
		glBeginQuery(GL_TIME_ELAPSED, m_RendererID);
	}

	void OpenGLQuery::End() const
	{
		glEndQuery(GL_TIME_ELAPSED);
	}

	uint64_t OpenGLQuery::GetQueryResults() const
	{
		GLuint64 result;
		glGetQueryObjectui64v(m_RendererID, GL_QUERY_RESULT, &result);

		return result;
	}
}
