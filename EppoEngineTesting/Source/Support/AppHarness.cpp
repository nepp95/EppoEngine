#include "pch.h"

#include "Support/AppHarness.h"

#include "Renderer/DeviceManager.h"

#include <exception>

namespace Eppo::Testing
{
	namespace
	{
		// The base ctor builds window/device/ImGui; wait for GPU idle before teardown
		// (Run() does the same after its loop, but a StepFrame harness must too).
		class HarnessApp final : public Application
		{
		public:
			HarnessApp()
				: Application(ApplicationParams{ .Args = CommandLineArgs(0, nullptr) })
			{}

			~HarnessApp() override
			{
				GetDeviceManager()->GetDevice()->waitForIdle();
			}
		};

		std::unique_ptr<HarnessApp> s_App;
		bool s_BootAttempted = false;
	}

	auto AppHarness::Get() -> Application*
	{
		if (!s_BootAttempted)
		{
			s_BootAttempted = true;
			try
			{
				s_App = std::make_unique<HarnessApp>();
			}
			catch (const std::exception& ex)
			{
				Log::Error("AppHarness: failed to boot application: {}", ex.what());
				s_App.reset();
			}
		}

		return s_App.get();
	}

	auto AppHarness::IsAvailable() -> bool
	{
		return Get() != nullptr;
	}

	auto AppHarness::AdvanceFrames(uint32_t count, float timestep) -> void
	{
		Application* app = Get();
		if (!app)
			return;

		for (uint32_t i = 0; i < count && app->IsRunning(); ++i)
			app->StepFrame(timestep);
	}

	auto AppHarness::Shutdown() -> void
	{
		s_App.reset();
		s_BootAttempted = false; // allow a fresh boot on a later Get()
	}
}
