#include "pch.h"
#include "Utility/Process.h"

#if defined(EP_PLATFORM_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#else
	#include <spawn.h>
	#include <sys/wait.h>

extern char** environ;
#endif

namespace Eppo
{
#if defined(EP_PLATFORM_WINDOWS)
	namespace
	{
		// Windows takes a single command line, not an argument vector, so each
		// argument has to be re-quoted the way CommandLineToArgvW parses it back:
		// backslashes are only special immediately before a quote, where they
		// double. Getting this wrong silently corrupts paths containing spaces.
		auto QuoteArgument(const std::string& arg) -> std::string
		{
			if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos)
				return arg;

			std::string quoted = "\"";
			for (auto it = arg.begin();; ++it)
			{
				size_t backslashes = 0;
				while (it != arg.end() && *it == '\\')
				{
					++it;
					++backslashes;
				}

				if (it == arg.end())
				{
					quoted.append(backslashes * 2, '\\');
					break;
				}

				quoted.append(*it == '"' ? backslashes * 2 + 1 : backslashes, '\\');
				quoted += *it;
			}

			return quoted + '"';
		}

		auto ToWide(const std::string& text) -> std::wstring
		{
			if (text.empty())
				return {};

			const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
			std::wstring wide(static_cast<size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);

			return wide;
		}

		class ScopedHandle
		{
		public:
			explicit ScopedHandle(HANDLE handle) : m_Handle(handle) {}
			~ScopedHandle()
			{
				if (m_Handle)
					CloseHandle(m_Handle);
			}

			ScopedHandle(const ScopedHandle&) = delete;
			ScopedHandle& operator=(const ScopedHandle&) = delete;
			ScopedHandle(ScopedHandle&&) = delete;
			ScopedHandle& operator=(ScopedHandle&&) = delete;

			[[nodiscard]] auto Get() const -> HANDLE { return m_Handle; }

		private:
			HANDLE m_Handle;
		};
	}

	auto RunProcess(const std::string& executable, const std::vector<std::string>& args) -> int
	{
		EP_PROFILE_FN("RunProcess");

		std::string commandLine = QuoteArgument(executable);
		for (const auto& arg : args)
			commandLine += " " + QuoteArgument(arg);

		// CreateProcessW writes into the command line buffer, so it cannot be const.
		std::wstring wideCommandLine = ToWide(commandLine);

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};

		if (!CreateProcessW(nullptr, wideCommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
			&startupInfo, &processInfo))
		{
			Log::Error("Failed to start '{}' (error {})", executable, GetLastError());
			return -1;
		}

		const ScopedHandle process(processInfo.hProcess);
		const ScopedHandle thread(processInfo.hThread);

		WaitForSingleObject(process.Get(), INFINITE);

		DWORD exitCode = 0;
		if (!GetExitCodeProcess(process.Get(), &exitCode))
		{
			Log::Error("Failed to read the exit code of '{}'", executable);
			return -1;
		}

		return static_cast<int>(exitCode);
	}
#else
	auto RunProcess(const std::string& executable, const std::vector<std::string>& args) -> int
	{
		EP_PROFILE_FN("RunProcess");

		std::vector<char*> argv;
		argv.reserve(args.size() + 2);
		argv.push_back(const_cast<char*>(executable.c_str()));
		for (const auto& arg : args)
			argv.push_back(const_cast<char*>(arg.c_str()));
		argv.push_back(nullptr);

		pid_t pid = 0;
		if (posix_spawnp(&pid, executable.c_str(), nullptr, nullptr, argv.data(), environ) != 0)
		{
			Log::Error("Failed to start '{}': {}", executable, strerror(errno));
			return -1;
		}

		int status = 0;
		if (waitpid(pid, &status, 0) == -1)
		{
			Log::Error("Failed to wait for '{}': {}", executable, strerror(errno));
			return -1;
		}

		// A child killed by a signal has no exit code of its own.
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
#endif
}
