#pragma once

#include <unordered_map>

#if defined(EPPO_ENABLE_PROFILING)
	#include <tracy/Tracy.hpp>
	#define EPPO_PROFILE_FUNCTION(name) ZoneScopedN(name)
	#define EPPO_PROFILE_FRAME_MARK FrameMark
	#define EPPO_PROFILE_GPU(context, commandBuffer, name) TracyVkZone(context, commandBuffer, name)
	#define EPPO_PROFILE_GPU_END(context, commandBuffer) TracyVkCollect(context, commandBuffer)
	#define EPPO_PROFILE_PLOT(name, var) TracyPlot(name, var)
#else
	#define EPPO_PROFILE_FUNCTION(name)
	#define EPPO_PROFILE_FRAME_MARK
	#define EPPO_PROFILE_GPU
	#define EPPO_PROFILE_GPU_END
	#define EPPO_PROFILE_PLOT(name, var)
#endif

namespace Eppo
{
	struct ProfileResult
	{
		std::string Tag;
		std::chrono::microseconds Time;

		ProfileResult(const std::string& tag, std::chrono::microseconds time)
			: Tag(tag), Time(time)
		{}
	};

	class Profiler : public RefCounter
	{
	public:
		Profiler() = default;
		~Profiler() = default;

		void AddProfile(const std::string& category, const ProfileResult& profileResult);
		void Clear();

		const std::unordered_map<std::string, std::vector<ProfileResult>>& GetProfileData() const { return m_ProfileData; }

	private:
		std::unordered_map<std::string, std::vector<ProfileResult>> m_ProfileData;
	};

	class ProfilerTimer
	{
	public:
		ProfilerTimer(const std::string& category, const std::string& tag);
		~ProfilerTimer();

	private:
		std::string m_Category;
		std::string m_Tag;

		std::chrono::time_point<std::chrono::steady_clock> m_StartTimePoint;
	};
}

#define EPPO_PROFILE_FN(category, name) ProfilerTimer timer__LINE__(category, name)
