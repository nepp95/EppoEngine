#pragma once

#include "Core/Log.h"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/pattern_formatter.h>

#include <mutex>
#include <vector>

namespace Eppo
{
	inline constexpr size_t LOG_BUFFER_CAPACITY = 8192;

	struct LogEntry
	{
		LogSource Source = LogSource::Core;
		spdlog::level::level_enum Level = spdlog::level::trace;
		std::string Text;
	};

	// Editor-only ring buffer of formatted log entries, attached to the loggers via
	// Log::AddSink. A shipped runtime never installs it, so it never retains logs.
	class LogSink final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit LogSink(const size_t capacity)
			: m_Entries(std::max<size_t>(capacity, 1))
		{
			formatter_ = std::make_unique<spdlog::pattern_formatter>("[%T.%e] [%n]: %v");
		}

		// Appends every entry logged since fromVersion to out and returns the version to pass in next time.
		auto CopySince(const uint64_t fromVersion, std::vector<LogEntry>& out) -> uint64_t
		{
			std::lock_guard lock(mutex_);

			const uint64_t missed = m_Version > fromVersion ? m_Version - fromVersion : 0;
			const size_t available = std::min<size_t>(missed, m_Count);
			const size_t capacity = m_Entries.size();

			for (size_t i = m_Count - available; i < m_Count; ++i)
				out.push_back(m_Entries[(m_Head + capacity - m_Count + i) % capacity]);

			return m_Version;
		}

	protected:
		auto sink_it_(const spdlog::details::log_msg& msg) -> void override
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

		auto flush_() -> void override {}

		// This sink owns its layout; the loggers sharing it must not repattern it.
		auto set_pattern_(const std::string&) -> void override {}
		auto set_formatter_(std::unique_ptr<spdlog::formatter>) -> void override {}

	private:
		static auto SourceFromLoggerName(const spdlog::string_view_t& name) -> LogSource
		{
			const std::string_view view(name.data(), name.size());

			if (view == "Glfw")
				return LogSource::Glfw;
			if (view == "Script")
				return LogSource::Script;
			if (view == "Vulkan")
				return LogSource::Vulkan;

			return LogSource::Core;
		}

		std::vector<LogEntry> m_Entries;
		size_t m_Head = 0;
		size_t m_Count = 0;
		uint64_t m_Version = 0;
	};
}
