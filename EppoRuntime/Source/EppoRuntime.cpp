#include "RuntimeLayer.h"

#define EP_CUSTOM_ENTRY_POINT
#include <EppoEngine.h>
#include "Core/EntryPoint.h"

#if defined(EP_PLATFORM_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#endif

namespace Eppo
{
	class RuntimeApplication final : public Application
	{
	public:
		explicit RuntimeApplication(ApplicationParams&& params)
			: Application(std::move(params))
		{
			PushLayer<RuntimeLayer>();
		}
	};

	auto CreateApplication(int, char**) -> Application*
	{
		const auto title = FS::GetExecutablePath().stem().string();

        const CommandLineArgs args(0, nullptr);
	    ApplicationParams params{
	        .Args = args,
	        .Title = title,
	        .VSync = true,
	        .Fullscreen = true,
	        .EnableImGui =
                #if defined(EP_DIST)
                false,
                #else
                true,
                #endif
            .EnableFileDialogs = false,
	    };

	    const auto app = new RuntimeApplication(std::move(params));

		return app;
	}

	auto PrepareRuntimeStorage() -> bool
	{
		const auto applicationName = FS::GetExecutablePath().stem().string();
		const auto writableDirectory = FS::GetUserStateDirectory(applicationName);
		if (writableDirectory.empty() || !FS::ConfigureWritableDirectory(writableDirectory))
		{
			ErrorDialog::Show("Eppo Runtime Error", "Failed to create the runtime writable directory.");
			return false;
		}
		return true;
	}

	auto RunRuntime() -> int
	{
		if (!PrepareRuntimeStorage())
			return 1;

		try
		{
			return RunApplication(0, nullptr);
		}
		catch (const std::exception& exception)
		{
			Log::Error("Runtime failed: {}", exception.what());
			ErrorDialog::Show("Eppo Runtime Error", exception.what());
			return 1;
		}
	}
}

#if defined(EP_PLATFORM_WINDOWS)
auto WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int
{
	return Eppo::RunRuntime();
}
#else
auto main(int, char**) -> int
{
	return Eppo::RunRuntime();
}
#endif
