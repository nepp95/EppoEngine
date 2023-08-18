#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

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

template<glm::length_t L, typename T, glm::qualifier Q>
struct fmt::formatter<glm::vec<L, T, Q>> : fmt::formatter<std::string>
{
	auto format(glm::vec<L, T, Q> vector, format_context& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), glm::to_string(vector));
	}
};

#define EPPO_TRACE(...)		::Eppo::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define EPPO_INFO(...)		::Eppo::Log::GetCoreLogger()->info(__VA_ARGS__)
#define EPPO_WARN(...)		::Eppo::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define EPPO_ERROR(...)		::Eppo::Log::GetCoreLogger()->error(__VA_ARGS__)
