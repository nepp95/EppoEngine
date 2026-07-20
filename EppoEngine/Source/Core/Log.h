#pragma once

#include "Core/UUID.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>
#include <mutex>
#include <vector>

namespace Eppo
{
	enum class LogSource
	{
		Core,
		Glfw,
	    Script,
		Vulkan
	};

	inline constexpr size_t LOG_BUFFER_CAPACITY = 8192;

	struct LogEntry
	{
		LogSource Source = LogSource::Core;
		spdlog::level::level_enum Level = spdlog::level::trace;
		std::string Text;
	};

	class LogBufferSink final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit LogBufferSink(size_t capacity);

		// Appends every entry logged since fromVersion to out and returns the version to pass in next time.
		auto CopySince(uint64_t fromVersion, std::vector<LogEntry>& out) -> uint64_t;

	protected:
		auto sink_it_(const spdlog::details::log_msg& msg) -> void override;
		auto flush_() -> void override {}

		// This sink owns its layout; the loggers sharing it must not repattern it.
		auto set_pattern_(const std::string&) -> void override {}
		auto set_formatter_(std::unique_ptr<spdlog::formatter>) -> void override {}

	private:
		std::vector<LogEntry> m_Entries;
		size_t m_Head = 0;
		size_t m_Count = 0;
		uint64_t m_Version = 0;
	};

	class Log
	{
	public:
		static auto Init() -> void;

		static auto GetBuffer() -> const std::shared_ptr<LogBufferSink>& { return s_BufferSink; }

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
		static std::shared_ptr<LogBufferSink> s_BufferSink;
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