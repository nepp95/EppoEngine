#include "pch.h"
#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Eppo
{
	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

	void Log::Init()
	{
		EPPO_PROFILE_FUNCTION("Log::Init");

		spdlog::set_pattern("%^[%T] [%n]: %v%$");

		s_CoreLogger = spdlog::stdout_color_mt("Engine");
		s_CoreLogger->set_level(spdlog::level::trace);
	}
}
