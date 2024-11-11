#pragma once

#include <deque>
#include <map>

namespace Eppo
{
	class GarbageCollector
	{
	public:
		GarbageCollector();

		void Update(uint32_t frameNumber);

		void SubmitFreeFn(std::function<void()> fn, bool freeOnShutdown = true);
		void Shutdown();

	private:
		uint32_t m_CurrentFrameNumber = 0;

		// Key being the frame number in which it is to be deleted
		// uint32_t gives us approximately 19.884 hours at 60 fps before we run out of frame numbers
		std::map<uint32_t, std::vector<std::function<void()>>> m_FreeFns;

		// Resources that should only be freed on shutdown
		std::deque<std::function<void()>> m_FreeFnsOnShutdown;

		static bool s_IsInstantiated;
	};
}
