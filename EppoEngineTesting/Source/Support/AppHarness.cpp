// The engine PCH (Ref/ScopedPtr aliases, Log, etc.) is not applied to test TUs,
// so pull it in explicitly before any engine header — Application.h names Ref<T>
// in its signatures.
#include "pch.h"

#include "Support/AppHarness.h"

#include "Renderer/DeviceManager.h"

#include <exception>
#include <memory>

namespace Eppo::Testing
{
	namespace
	{
		// Minimal concrete application for the harness. The base constructor
		// already creates the window, Vulkan device, and ImGui layer; we push no
		// editor layers. Run() waits for the GPU to idle after its loop, so a
		// StepFrame-driven harness must do the same before teardown — hence the
		// waitForIdle in the destructor, which runs before the base class tears
		// the device down.
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
		// Allow a fresh boot on a later Get() (Application::~Application clears its
		// singleton, so re-construction is legal). The runner calls this once at
		// exit, but keeping it reusable avoids a surprising permanently-dead state.
		s_BootAttempted = false;
	}
}
