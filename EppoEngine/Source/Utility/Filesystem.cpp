#include "pch.h"
#include "Utility/Filesystem.h"

#if defined(EP_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#elif defined(EP_PLATFORM_LINUX)
    #include <unistd.h>
    #include <climits>
#endif

namespace Eppo::FS
{
    auto GetExecutableDirectory() -> std::filesystem::path
    {
        // Queried once from the OS and cached. Falls back to the working directory
        // if the platform query fails, preserving the previous behaviour.
        static const std::filesystem::path directory = []() -> std::filesystem::path
        {
            #if defined(EP_PLATFORM_WINDOWS)
            std::wstring buffer(MAX_PATH, L'\0');
            auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            while (length != 0 && length == buffer.size())
            {
                // Truncated (long path): grow and retry.
                buffer.resize(buffer.size() * 2);
                length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            }
            if (length == 0)
                return std::filesystem::current_path();
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
            #elif defined(EP_PLATFORM_LINUX)
            char buffer[PATH_MAX];
            const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer));
            if (length <= 0)
                return std::filesystem::current_path();
            return std::filesystem::path(std::string(buffer, static_cast<size_t>(length))).parent_path();
            #else
            return std::filesystem::current_path();
            #endif
        }();

        return directory;
    }
}
