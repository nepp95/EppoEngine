#include "pch.h"
#include "CommandQueue.h"

namespace Eppo
{
	CommandQueue::CommandQueue(bool isMultiThreaded)
		: m_IsMultiThreaded(isMultiThreaded)
	{}

	void CommandQueue::AddCommand(RenderCommand fn)
	{
		EPPO_PROFILE_FUNCTION("RenderCommandQueue::AddCommand");

		m_CommandQueue.push(std::move(fn));
		m_CommandCount++;
	}

	void CommandQueue::Execute()
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
