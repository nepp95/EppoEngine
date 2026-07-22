#include "pch.h"
#include "Core/Log.h"

#include "Utility/Filesystem.h"

#include <spdlog/pattern_formatter.h>

namespace
{
	auto SourceFromLoggerName(const spdlog::string_view_t& name) -> Eppo::LogSource
	{
		const std::string_view view(name.data(), name.size());

		if (view == "Glfw")
			return Eppo::LogSource::Glfw;
		if (view == "Script")
			return Eppo::LogSource::Script;
		if (view == "Vulkan")
			return Eppo::LogSource::Vulkan;

		return Eppo::LogSource::Core;
	}
}

namespace Eppo
{
	LogBufferSink::LogBufferSink(const size_t capacity)
		: m_Entries(std::max<size_t>(capacity, 1))
	{
		formatter_ = std::make_unique<spdlog::pattern_formatter>("[%T.%e] [%n]: %v");
	}

	auto LogBufferSink::CopySince(const uint64_t fromVersion, std::vector<LogEntry>& out) -> uint64_t
	{
		std::lock_guard lock(mutex_);

		const uint64_t missed = m_Version > fromVersion ? m_Version - fromVersion : 0;
		const size_t available = std::min<size_t>(missed, m_Count);
		const size_t capacity = m_Entries.size();

		for (size_t i = m_Count - available; i < m_Count; ++i)
			out.push_back(m_Entries[(m_Head + capacity - m_Count + i) % capacity]);

		return m_Version;
	}

	auto LogBufferSink::sink_it_(const spdlog::details::log_msg& msg) -> void
	{
		spdlog::memory_buf_t formatted;
		formatter_->format(msg, formatted);

		std::string_view text(formatted.data(), formatted.size());
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
			text.remove_suffix(1);

		// Assigning into the evicted slot reuses its string capacity, so a settled buffer stops allocating.
		LogEntry& entry = m_Entries[m_Head];
		entry.Source = SourceFromLoggerName(msg.logger_name);
		entry.Level = msg.level;
		entry.Text.assign(text);

		m_Head = (m_Head + 1) % m_Entries.size();
		if (m_Count < m_Entries.size())
			++m_Count;
		++m_Version;
	}

	Ref<spdlog::sinks::basic_file_sink_mt> Log::s_FileLoggerSink = nullptr;
	Ref<spdlog::sinks::stdout_color_sink_mt> Log::s_ConsoleLoggerSink = nullptr;
	Ref<LogBufferSink> Log::s_BufferSink = nullptr;
	Ref<spdlog::logger> Log::s_CoreLogger = nullptr;
	Ref<spdlog::logger> Log::s_GlfwLogger = nullptr;
    Ref<spdlog::logger> Log::s_ScriptLogger = nullptr;
	Ref<spdlog::logger> Log::s_VulkanLogger = nullptr;

	auto Log::Init() -> void
	{
		const auto logDirectory = FS::GetWritableDirectory();
		std::filesystem::create_directories(logDirectory);
		const auto latestLog = logDirectory / "latest.log";
		const auto previousLog = logDirectory / "previous.log";
		if (std::filesystem::exists(latestLog))
		{
			if (std::filesystem::exists(previousLog))
				std::filesystem::remove(previousLog);
			std::filesystem::rename(latestLog, previousLog);
		}

		s_FileLoggerSink = CreateRef<spdlog::sinks::basic_file_sink_mt>(latestLog.string(), true);
		s_FileLoggerSink->set_level(spdlog::level::trace);
		s_ConsoleLoggerSink = CreateRef<spdlog::sinks::stdout_color_sink_mt>();
		s_ConsoleLoggerSink->set_level(spdlog::level::trace);
		s_BufferSink = CreateRef<LogBufferSink>(LOG_BUFFER_CAPACITY);
		s_BufferSink->set_level(spdlog::level::trace);

		const spdlog::sinks_init_list sinks = { s_FileLoggerSink, s_ConsoleLoggerSink, s_BufferSink };

		s_CoreLogger = CreateRef<spdlog::logger>("Core", sinks);
		s_CoreLogger->set_level(spdlog::level::trace);
		s_GlfwLogger = CreateRef<spdlog::logger>("Glfw", sinks);
		s_GlfwLogger->set_level(spdlog::level::trace);
	    s_ScriptLogger = CreateRef<spdlog::logger>("Script", sinks);
	    s_ScriptLogger->set_level(spdlog::level::trace);
		s_VulkanLogger = CreateRef<spdlog::logger>("Vulkan", sinks);
		s_VulkanLogger->set_level(spdlog::level::trace);

		spdlog::set_default_logger(s_CoreLogger);
		s_CoreLogger->set_pattern("%^[%T.%e] [%n]: %v%$");
	}
}
