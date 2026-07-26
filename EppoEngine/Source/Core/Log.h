#pragma once

#include "Core/UUID.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>

namespace Eppo
{
	enum class LogSource
	{
		Core,
		Glfw,
	    Script,
		Vulkan
	};

	class Log
	{
	public:
		static auto Init() -> void;

		// Attach an extra sink to every logger. The editor uses this to feed its log panel;
		// non-editor hosts never do, so they don't pay for in-memory log retention.
		static auto AddSink(const spdlog::sink_ptr& sink) -> void;

		template<typename... Args>
		static constexpr auto Trace(fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			Trace(LogSource::Core, fmt, std::forward<Args>(args)...);
		}

		template<typename... Args>
		static constexpr auto Trace(const LogSource source, fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			switch (source)
			{
				case LogSource::Glfw:
				{
					s_GlfwLogger->trace(fmt, std::forward<Args>(args)...);
					break;
				}

			    case LogSource::Script:
				{
				    s_ScriptLogger->trace(fmt, std::forward<Args>(args)...);
				    break;
				}

				case LogSource::Vulkan:
				{
					s_VulkanLogger->trace(fmt, std::forward<Args>(args)...);
					break;
				}

				default:
				{
					s_CoreLogger->trace(fmt, std::forward<Args>(args)...);
					break;
				}
			}
		}

		template<typename... Args>
		static constexpr auto Info(fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			Info(LogSource::Core, fmt, std::forward<Args>(args)...);
		}

		template<typename... Args>
		static constexpr auto Info(const LogSource source, fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			switch (source)
			{
				case LogSource::Glfw:
				{
					s_GlfwLogger->info(fmt, std::forward<Args>(args)...);
					break;
				}

			    case LogSource::Script:
				{
				    s_ScriptLogger->info(fmt, std::forward<Args>(args)...);
				    break;
				}

				case LogSource::Vulkan:
				{
					s_VulkanLogger->info(fmt, std::forward<Args>(args)...);
					break;
				}

				default:
				{
					s_CoreLogger->info(fmt, std::forward<Args>(args)...);
					break;
				}
			}
		}

		template<typename... Args>
		static constexpr auto Warn(fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			Warn(LogSource::Core, fmt, std::forward<Args>(args)...);
		}

		template<typename... Args>
		static constexpr auto Warn(const LogSource source, fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			switch (source)
			{
				case LogSource::Glfw:
				{
					s_GlfwLogger->warn(fmt, std::forward<Args>(args)...);
					break;
				}

			    case LogSource::Script:
				{
				    s_ScriptLogger->warn(fmt, std::forward<Args>(args)...);
				    break;
				}

				case LogSource::Vulkan:
				{
					s_VulkanLogger->warn(fmt, std::forward<Args>(args)...);
					break;
				}

				default:
				{
					s_CoreLogger->warn(fmt, std::forward<Args>(args)...);
					break;
				}
			}
		}

		template<typename... Args>
		static constexpr auto Error(fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			Error(LogSource::Core, fmt, std::forward<Args>(args)...);
		}

		template<typename... Args>
		static constexpr auto Error(const LogSource source, fmt::format_string<Args...> fmt, Args&&... args) -> void
		{
			switch (source)
			{
				case LogSource::Glfw:
				{
					s_GlfwLogger->error(fmt, std::forward<Args>(args)...);
					break;
				}

			    case LogSource::Script:
				{
				    s_ScriptLogger->error(fmt, std::forward<Args>(args)...);
				    break;
				}

				case LogSource::Vulkan:
				{
					s_VulkanLogger->error(fmt, std::forward<Args>(args)...);
					break;
				}

				default:
				{
					s_CoreLogger->error(fmt, std::forward<Args>(args)...);
					break;
				}
			}
		}

	private:
		static std::shared_ptr<spdlog::sinks::basic_file_sink_mt> s_FileLoggerSink;
		static std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> s_ConsoleLoggerSink;
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_GlfwLogger;
	    static std::shared_ptr<spdlog::logger> s_ScriptLogger;
		static std::shared_ptr<spdlog::logger> s_VulkanLogger;
	};
}

template<>
struct fmt::formatter<std::filesystem::path> : formatter<std::string_view>
{
	auto format(const std::filesystem::path& v, format_context& ctx) const -> format_context::iterator
	{
		return formatter<std::string_view>::format(v.string(), ctx);
	}
};

template<>
struct fmt::formatter<Eppo::UUID> : formatter<uint64_t>
{
	auto format(const Eppo::UUID& v, format_context& ctx) const -> format_context::iterator
	{
		return formatter<uint64_t>::format(static_cast<uint64_t>(v), ctx);
	}
};