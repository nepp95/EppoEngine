#pragma once

#include "Core/UUID.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <filesystem>

namespace Eppo
{
    // TODO: Refactor because this can be done with single loggers. Refer to docs.
    class Log
    {
    public:
        static void Init();

        static Ref<spdlog::logger>& GetCoreLogger()
        {
            return s_CoreLogger;
        }

        static Ref<spdlog::logger>& GetScriptLogger()
        {
            return s_ScriptLogger;
        }

    private:
        static Ref<spdlog::logger> s_CoreLogger;
        static Ref<spdlog::logger> s_ScriptLogger;
    };
}

template<>
struct fmt::formatter<Eppo::UUID> : formatter<uint64_t>
{
    auto format(const Eppo::UUID& v, format_context& ctx) const -> format_context::iterator
    {
        return formatter<uint64_t>::format(static_cast<uint64_t>(v), ctx);
    }
};

template<>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view>
{
    auto format(const std::filesystem::path& v, format_context& ctx) const -> format_context::iterator
    {
        return formatter<std::string_view>::format(v.string(), ctx);
    }
};

template<glm::length_t L, typename T, glm::precision Q>
struct fmt::formatter<glm::vec<L, T, Q>> : formatter<std::string>
{
    auto format(glm::vec<L, T, Q> v, format_context& ctx) const -> format_context::iterator
    {
        return formatter<std::string>::format(glm::to_string(v), ctx);
    }
};

#define EPPO_TRACE(...) ::Eppo::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define EPPO_INFO(...) ::Eppo::Log::GetCoreLogger()->info(__VA_ARGS__)
#define EPPO_WARN(...) ::Eppo::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define EPPO_ERROR(...) ::Eppo::Log::GetCoreLogger()->error(__VA_ARGS__)

#if defined(EPPO_TRACK_MEMORY)
    #define EPPO_MEM_WARN(...) ::Eppo::Log::GetCoreLogger()->trace(__VA_ARGS__)
#else
    #define EPPO_MEM_WARN(...)
#endif

#define EPPO_SCRIPT_TRACE(...) ::Eppo::Log::GetScriptLogger()->trace(__VA_ARGS__)
#define EPPO_SCRIPT_INFO(...) ::Eppo::Log::GetScriptLogger()->info(__VA_ARGS__)
#define EPPO_SCRIPT_WARN(...) ::Eppo::Log::GetScriptLogger()->warn(__VA_ARGS__)
#define EPPO_SCRIPT_ERROR(...) ::Eppo::Log::GetScriptLogger()->error(__VA_ARGS__)
