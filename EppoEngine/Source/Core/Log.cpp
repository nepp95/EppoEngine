#include "pch.h"
#include "Core/Log.h"

#include "Utility/Filesystem.h"

namespace Eppo
{
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> Log::s_FileLoggerSink = nullptr;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> Log::s_ConsoleLoggerSink = nullptr;
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger = nullptr;
    std::shared_ptr<spdlog::logger> Log::s_GlfwLogger = nullptr;
    std::shared_ptr<spdlog::logger> Log::s_ScriptLogger = nullptr;
    std::shared_ptr<spdlog::logger> Log::s_VulkanLogger = nullptr;
    std::shared_ptr<spdlog::logger> Log::s_DX12Logger = nullptr;

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

        s_FileLoggerSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(latestLog.string(), true);
        s_FileLoggerSink->set_level(spdlog::level::trace);
        s_ConsoleLoggerSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        s_ConsoleLoggerSink->set_level(spdlog::level::trace);

        const spdlog::sinks_init_list sinks = { s_FileLoggerSink, s_ConsoleLoggerSink };

        s_CoreLogger = std::make_shared<spdlog::logger>("Core", sinks);
        s_CoreLogger->set_level(spdlog::level::trace);
        s_GlfwLogger = std::make_shared<spdlog::logger>("Glfw", sinks);
        s_GlfwLogger->set_level(spdlog::level::trace);
        s_ScriptLogger = std::make_shared<spdlog::logger>("Script", sinks);
        s_ScriptLogger->set_level(spdlog::level::trace);
        s_VulkanLogger = std::make_shared<spdlog::logger>("Vulkan", sinks);
        s_VulkanLogger->set_level(spdlog::level::trace);
        s_DX12Logger = std::make_shared<spdlog::logger>("DX12", sinks);
        s_DX12Logger->set_level(spdlog::level::trace);

        spdlog::set_default_logger(s_CoreLogger);
        s_CoreLogger->set_pattern("%^[%T.%e] [%n]: %v%$");
    }

    auto Log::AddSink(const spdlog::sink_ptr& sink) -> void
    {
        for (const auto& logger : { s_CoreLogger, s_GlfwLogger, s_ScriptLogger, s_VulkanLogger })
            logger->sinks().push_back(sink);
    }
}
