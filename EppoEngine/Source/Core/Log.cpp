#include "pch.h"
#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Eppo
{
	Ref<spdlog::logger> Log::s_CoreLogger;
	Ref<spdlog::logger> Log::s_ScriptLogger;

	void Log::Init()
	{
		spdlog::set_pattern("%^[%T.%e] [%n]: %v%$");

		s_CoreLogger = spdlog::stdout_color_mt("Engine");
		s_CoreLogger->set_level(spdlog::level::trace);

		s_ScriptLogger = spdlog::stdout_color_mt("Script");
		s_ScriptLogger->set_level(spdlog::level::trace);
	}
}
