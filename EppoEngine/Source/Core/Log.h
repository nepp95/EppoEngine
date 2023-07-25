#pragma once

#include <spdlog/spdlog.h>

namespace Eppo
{
	class Log
	{
	public:
		static void Init();

		static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }

	private:
		static Ref<spdlog::logger> s_CoreLogger;
	};
}

#define EPPO_TRACE(...)		::Eppo::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define EPPO_INFO(...)		::Eppo::Log::GetCoreLogger()->info(__VA_ARGS__)
#define EPPO_WARN(...)		::Eppo::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define EPPO_ERROR(...)		::Eppo::Log::GetCoreLogger()->error(__VA_ARGS__)