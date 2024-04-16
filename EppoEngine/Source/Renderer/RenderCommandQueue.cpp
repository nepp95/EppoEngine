#include "pch.h"
#include "RenderCommandQueue.h"

#include <glad/glad.h>
#include <tracy/TracyOpenGL.hpp>

namespace Eppo
{
	void RenderCommandQueue::AddCommand(RenderCommand fn)
	{
		EPPO_PROFILE_FUNCTION("RenderCommandQueue::AddCommand");

		m_CommandQueue.push(fn);
		m_CommandCount++;
	}

	void RenderCommandQueue::Execute()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandQueue::Execute");

		for (uint32_t i = 0; i < m_CommandCount; i++)
		{
			m_CommandQueue.front()();
			m_CommandQueue.pop();
		}

		m_CommandCount = 0;
	}
}
