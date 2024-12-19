#include "pch.h"
#include "GarbageCollector.h"

namespace Eppo
{
	bool GarbageCollector::s_IsInstantiated = false;

	GarbageCollector::GarbageCollector()
	{
		EPPO_ASSERT(!s_IsInstantiated)
		s_IsInstantiated = true;
	}

	void GarbageCollector::Update(const uint32_t frameNumber)
	{
		m_CurrentFrameNumber = frameNumber;

		for (auto it = m_FreeFns.begin(); it != m_FreeFns.lower_bound(m_CurrentFrameNumber);)
		{
			for (const auto& fn : it->second)
				fn();

			it = m_FreeFns.erase(it);
		}
	}

	void GarbageCollector::SubmitFreeFn(std::function<void()> fn, const bool freeOnShutdown)
	{
		if (freeOnShutdown)
			m_FreeFnsOnShutdown.emplace_back(fn);
		else
			m_FreeFns[m_CurrentFrameNumber + 3].emplace_back(fn);
	}

	void GarbageCollector::Shutdown()
	{
		// First deallocate resources that were dynamic (to be deallocated in frame X)
		for (const auto& [frameNumber, fns] : m_FreeFns)
		{
			// Execute free function
			for (const auto& fn : fns)
				fn();
		}

		// Secondly deallocate resources that were allocated for the entire lifetime
		for (auto it = m_FreeFnsOnShutdown.rbegin(); it != m_FreeFnsOnShutdown.rend(); ++it)
			(*it)();
	}
}
