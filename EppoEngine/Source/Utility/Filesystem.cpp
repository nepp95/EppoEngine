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
	namespace
	{
		std::filesystem::path s_WritableDirectory;
	}

    auto GetExecutablePath() -> std::filesystem::path
    {
        static const std::filesystem::path executable = []() -> std::filesystem::path
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
                return std::filesystem::current_path() / "EppoEngine";
            buffer.resize(length);
            return std::filesystem::path(buffer);
            #elif defined(EP_PLATFORM_LINUX)
            char buffer[PATH_MAX];
            const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer));
            if (length <= 0)
                return std::filesystem::current_path() / "EppoEngine";
            return std::filesystem::path(std::string(buffer, static_cast<size_t>(length)));
            #else
            return std::filesystem::current_path() / "EppoEngine";
            #endif
        }();

        return executable;
    }

	auto GetExecutableDirectory() -> std::filesystem::path
	{
		return GetExecutablePath().parent_path();
	}

	auto ConfigureWritableDirectory(const std::filesystem::path& path) -> bool
	{
		if (path.empty())
		{
			s_WritableDirectory.clear();
			return true;
		}

		const auto normalized = std::filesystem::absolute(path).lexically_normal();
		std::filesystem::create_directories(normalized);

		s_WritableDirectory = normalized;
		return true;
	}

	auto GetWritableDirectory() -> std::filesystem::path
	{
		return s_WritableDirectory.empty() ? std::filesystem::current_path() : s_WritableDirectory;
	}

	auto GetShaderCacheDirectory() -> std::filesystem::path
	{
		const auto cacheDirectory = s_WritableDirectory.empty()
			? GetResourcesDirectory() / "Shaders" / "Cache"
			: s_WritableDirectory / "ShaderCache";
		std::filesystem::create_directories(cacheDirectory);
		return cacheDirectory;
	}

	auto GetUserStateDirectory(const std::string& applicationName) -> std::filesystem::path
	{
		if (applicationName.empty())
			return {};

		#if defined(EP_PLATFORM_WINDOWS)
		char* localAppData = nullptr;
		size_t length = 0;
		if (_dupenv_s(&localAppData, &length, "LOCALAPPDATA") != 0 || !localAppData)
			return {};
		const std::filesystem::path directory = std::filesystem::path(localAppData) / applicationName;
		std::free(localAppData);
		return directory;
		#elif defined(EP_PLATFORM_LINUX)
		if (const char* stateHome = std::getenv("XDG_STATE_HOME"); stateHome && *stateHome)
			return std::filesystem::path(stateHome) / applicationName;
		const char* home = std::getenv("HOME");
		return home ? std::filesystem::path(home) / ".local" / "state" / applicationName : std::filesystem::path{};
		#else
		return {};
		#endif
	}
}
