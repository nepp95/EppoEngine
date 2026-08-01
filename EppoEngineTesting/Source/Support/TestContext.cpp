#include "pch.h"

#include "Support/TestContext.h"
#include "Support/AppHarness.h"
#include "Support/ScenarioLayer.h"

namespace Eppo::Testing
{
    namespace
    {
        // One ScenarioLayer pushed per app instance (keyed on the owner so a
        // re-booted harness gets a fresh layer, not a dangling pointer).
        auto GetScenarioLayer() -> ScenarioLayer*
        {
            static ScenarioLayer* layer = nullptr;
            static Application* owner = nullptr;

            Application* app = AppHarness::Get();
            if (!app)
                return nullptr;

            if (app != owner)
            {
                layer = app->PushLayer<ScenarioLayer>().get();
                owner = app;
            }

            return layer;
        }
    }

    TestContext::TestContext()
        : m_Scene(CreateRef<Scene>())
    {
        // Boot the shared app (no-op if already up).
        (void)AppHarness::Get();
    }

    TestContext::~TestContext() = default;

    auto TestContext::IsAvailable() const -> bool
    {
        return AppHarness::IsAvailable();
    }

    auto TestContext::AdvanceFrames(uint32_t count, const std::function<void(float)>& perFrame, float timestep) -> void
    {
        if (!AppHarness::IsAvailable())
            return;

        // The layer's OnUpdate runs `perFrame`, so it only fires on real stepped frames.
        ScenarioLayer* layer = GetScenarioLayer();
        if (layer)
            layer->SetUpdate(perFrame);

        AppHarness::AdvanceFrames(count, timestep);

        if (layer)
            layer->ClearUpdate();
    }
}
