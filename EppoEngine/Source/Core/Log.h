#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Eppo
{
	// TODO: Refactor because this can be done with single loggers. Refer to docs.
	class Log
	{
	public:
		static void Init();

		static Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static Ref<spdlog::logger>& GetScriptLogger() { return s_ScriptLogger; }

	private:
		static Ref<spdlog::logger> s_CoreLogger;
		static Ref<spdlog::logger> s_ScriptLogger;
	};
}

template<glm::length_t L, typename T, glm::qualifier Q>
struct fmt::formatter<glm::vec<L, T, Q>> : fmt::formatter<std::string>
{
	auto format(glm::vec<L, T, Q> vector, format_context& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), glm::to_string(vector));
	}
};

#define EPPO_TRACE(...)			::Eppo::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define EPPO_INFO(...)			::Eppo::Log::GetCoreLogger()->info(__VA_ARGS__)
#define EPPO_WARN(...)			::Eppo::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define EPPO_ERROR(...)			::Eppo::Log::GetCoreLogger()->error(__VA_ARGS__)

#define EPPO_SCRIPT_TRACE(...)	::Eppo::Log::GetScriptLogger()->trace(__VA_ARGS__)
#define EPPO_SCRIPT_INFO(...)	::Eppo::Log::GetScriptLogger()->info(__VA_ARGS__)
#define EPPO_SCRIPT_WARN(...)	::Eppo::Log::GetScriptLogger()->warn(__VA_ARGS__)
#define EPPO_SCRIPT_ERROR(...)	::Eppo::Log::GetScriptLogger()->error(__VA_ARGS__)
