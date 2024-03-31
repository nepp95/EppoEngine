#include "pch.h"
#include "Profiler.h"

#include "Core/Application.h"

namespace Eppo
{
	void Profiler::AddProfile(const std::string& category, const ProfileResult& profileResult)
	{
		m_ProfileData[category].push_back(profileResult);
	}

	void Profiler::Clear()
	{
		m_ProfileData.clear();
	}

	ProfilerTimer::ProfilerTimer(const std::string& category, const std::string& tag)
		: m_Category(category), m_Tag(tag)
	{
		m_StartTimePoint = std::chrono::steady_clock::now();
	}

	ProfilerTimer::~ProfilerTimer()
	{
		using namespace std::chrono;

		auto endTimePoint = steady_clock::now();

		//auto highResStart = std::chrono::duration<double, std::micro>(m_StartTimePoint.time_since_epoch());
		auto elapsedTime = time_point_cast<microseconds>(endTimePoint).time_since_epoch() -
			time_point_cast<microseconds>(m_StartTimePoint).time_since_epoch();

		ProfileResult result(m_Tag, elapsedTime);
		Application::Get().GetProfiler()->AddProfile(m_Category, result);
	}
}
