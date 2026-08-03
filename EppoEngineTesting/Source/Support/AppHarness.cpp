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
            explicit HarnessApp(ApplicationParams&& params)
                : Application(std::move(params))
            {}

            ~HarnessApp() override { GetDeviceManager()->GetDevice()->waitForIdle(); }
        };

        std::unique_ptr<HarnessApp> s_App;
        bool s_BootAttempted = false;
        bool s_UsesDefaultParams = true;

        auto GetOrCreateApp(ApplicationParams params) -> Application*
        {
            if (!s_BootAttempted)
            {
                s_BootAttempted = true;
                try
                {
                    s_App = std::make_unique<HarnessApp>(std::move(params));
                }
                catch (const std::exception& ex)
                {
                    Log::Error("AppHarness: failed to boot application: {}", ex.what());
                    s_App.reset();
                }
            }

            return s_App.get();
        }
    }

    auto AppHarness::Get() -> Application*
    {
        if (!s_UsesDefaultParams)
            Shutdown();

        return GetOrCreateApp(ApplicationParams{ .Args = CommandLineArgs(0, nullptr) });
    }

    auto AppHarness::Get(ApplicationParams params) -> Application*
    {
        if (!s_BootAttempted)
            s_UsesDefaultParams = false;

        return GetOrCreateApp(std::move(params));
    }

    auto AppHarness::IsAvailable() -> bool
    {
        return Get() != nullptr;
    }

    auto AppHarness::AdvanceFrames(uint32_t count, float timestep) -> void
    {
        Application* app = s_BootAttempted ? s_App.get() : Get();
        if (!app)
            return;

        for (uint32_t i = 0; i < count && app->IsRunning(); ++i)
            app->StepFrame(timestep);
    }

    auto AppHarness::Shutdown() -> void
    {
        s_App.reset();
        s_BootAttempted = false; // allow a fresh boot on a later Get()
        s_UsesDefaultParams = true;
    }
}
