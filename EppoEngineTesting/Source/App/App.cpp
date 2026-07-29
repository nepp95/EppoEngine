#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

using namespace Eppo;

// Foundation for the scenario tests: boots the real Application and steps frames.
// Needs a display + GPU (label `graphical`, excluded on headless CI).
SUITE(App)
{
    namespace
    {
        class UITrackingLayer : public Layer
        {
        public:
            auto OnUpdate(float) -> void override { UpdateCalled = true; }
            auto OnUIRender() -> void override { UIRenderCalled = true; }

            auto OnEvent(Event& event) -> void override
            {
                EventDispatcher dispatcher(event);
                dispatcher.Dispatch<WindowResizeEvent>([this](const WindowResizeEvent& resizeEvent)
                {
                    ResizeWidth = resizeEvent.GetWidth();
                    ResizeHeight = resizeEvent.GetHeight();
                    return false;
                });
            }

            bool UpdateCalled = false;
            bool UIRenderCalled = false;
            uint32_t ResizeWidth = 0;
            uint32_t ResizeHeight = 0;
        };
    }

    TEST(Application_Boot_ProducesWindowAndDevice)
    {
        Application* app = Testing::AppHarness::Get();
        REQUIRE CHECK(app != nullptr);

        // Bring-up must have produced a live window and device.
        CHECK(app->GetWindow() != nullptr);
        CHECK(app->GetDeviceManager() != nullptr);
        CHECK(app->IsRunning());
    }

    TEST(Application_AdvanceFrames_StaysRunning)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // Drive a batch of real frames; the app must survive and keep running
        // (nothing in this bare harness requests close).
        Testing::AppHarness::AdvanceFrames(30);

        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(Application_RepeatedAdvance_StaysRunning)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // A second batch reuses the same persisted app instance (boot-once per
        // process); stepping again must not crash or tear anything down.
        Testing::AppHarness::AdvanceFrames(15);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(Application_CustomWindowAndVSyncParameters_AreHonored)
    {
        Testing::AppHarness::Shutdown();
        ApplicationParams params{
            .Args = CommandLineArgs(0, nullptr),
            .Title = "Parameter Test",
            .Width = 1024,
            .Height = 640,
            .VSync = true,
        };
        Application* app = Testing::AppHarness::Get(std::move(params));
        REQUIRE CHECK(app != nullptr);

        CHECK_EQUAL(std::string("Parameter Test"), app->GetWindow()->GetSpecification().Title);
        CHECK_EQUAL(1024, app->GetWindow()->GetWidth());
        CHECK_EQUAL(640, app->GetWindow()->GetHeight());
        CHECK(app->GetDeviceManager()->GetParams().VSync);
    }

    TEST(Application_WithoutImGui_UpdatesLayersWithoutUIRender)
    {
        Testing::AppHarness::Shutdown();
        ApplicationParams params{
            .Args = CommandLineArgs(0, nullptr),
            .EnableImGui = false,
        };
        Application* app = Testing::AppHarness::Get(std::move(params));
        REQUIRE CHECK(app != nullptr);
        CHECK(app->GetImGuiLayer() == nullptr);

        const Ref<UITrackingLayer> layer = app->PushLayer<UITrackingLayer>();
        Testing::AppHarness::AdvanceFrames(3);
        CHECK(layer->UpdateCalled);
        CHECK(!layer->UIRenderCalled);
        CHECK(app->IsRunning());
    }

    TEST(Application_FullscreenUsesMonitorFramebufferSize)
    {
        Testing::AppHarness::Shutdown();
        ApplicationParams params{
            .Args = CommandLineArgs(0, nullptr),
            .Fullscreen = true,
            .EnableImGui = false,
        };
        Application* app = Testing::AppHarness::Get(std::move(params));
        REQUIRE CHECK(app != nullptr);
        CHECK(app->GetWindow()->GetSpecification().Fullscreen);
        const auto [width, height] = app->GetWindow()->GetFramebufferSize();
        CHECK(width > 0);
        CHECK(height > 0);
        CHECK_EQUAL(width, app->GetWindow()->GetWidth());
        CHECK_EQUAL(height, app->GetWindow()->GetHeight());
    }

    TEST(Application_WindowResizePropagatesToLayers)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        Application* app = Testing::AppHarness::Get();
        const Ref<UITrackingLayer> layer = app->PushLayer<UITrackingLayer>();
        WindowResizeEvent event(1280, 720);
        app->OnEvent(event);

        CHECK(!event.Handled);
        CHECK_EQUAL(1280, layer->ResizeWidth);
        CHECK_EQUAL(720, layer->ResizeHeight);
    }
}
