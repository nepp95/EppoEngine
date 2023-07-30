#include "pch.h"
#include "RenderCommandQueue.h"

namespace Eppo
{
	void RenderCommandQueue::AddCommand(RenderCommand fn)
	{
		m_CommandQueue.push(fn);
		m_CommandCount++;
	}

	void RenderCommandQueue::Execute()
	{
		for (uint32_t i = 0; i < m_CommandCount; i++)
		{
			m_CommandQueue.front()();
			m_CommandQueue.pop();
		}

		m_CommandCount = 0;
	}
}
