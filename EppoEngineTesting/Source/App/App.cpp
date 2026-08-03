#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include "Event/KeyEvent.h"
#include "ImGui/ImGuiLayer.h"

#include <imgui.h>

using namespace Eppo;

// Foundation for the scenario tests: boots the real Application and steps frames.
// Needs a display + GPU (label `graphical`, excluded on headless CI).
class UITrackingLayer : public Layer
{
public:
    auto OnUpdate(float) -> void override { UpdateCalled = true; }
    auto OnUIRender() -> void override { UIRenderCalled = true; }

    auto OnEvent(Event& event) -> void override
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>(
            [this](const WindowResizeEvent& resizeEvent)
            {
                ResizeWidth = resizeEvent.GetWidth();
                ResizeHeight = resizeEvent.GetHeight();
                return false;
            }
        );
    }

    bool UpdateCalled = false;
    bool UIRenderCalled = false;
    uint32_t ResizeWidth = 0;
    uint32_t ResizeHeight = 0;
};

TEST(App, Application_Boot_ProducesWindowAndDevice)
{
    Application* app = Testing::AppHarness::Get();
    EP_REQUIRE(app != nullptr);

    // Bring-up must have produced a live window and device.
    EXPECT_TRUE(app->GetWindow() != nullptr);
    EXPECT_TRUE(app->GetDeviceManager() != nullptr);
    EXPECT_TRUE(app->IsRunning());
}

TEST(App, Application_AdvanceFrames_StaysRunning)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    // Drive a batch of real frames; the app must survive and keep running
    // (nothing in this bare harness requests close).
    Testing::AppHarness::AdvanceFrames(30);

    EXPECT_TRUE(Testing::AppHarness::Get()->IsRunning());
}

TEST(App, Application_RepeatedAdvance_StaysRunning)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    // A second batch reuses the same persisted app instance (boot-once per
    // process); stepping again must not crash or tear anything down.
    Testing::AppHarness::AdvanceFrames(15);
    EXPECT_TRUE(Testing::AppHarness::Get()->IsRunning());
}

TEST(App, Application_CustomWindowAndVSyncParameters_AreHonored)
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
    EP_REQUIRE(app != nullptr);

    EXPECT_EQ(std::string("Parameter Test"), app->GetWindow()->GetSpecification().Title);
    EXPECT_EQ(1024, app->GetWindow()->GetWidth());
    EXPECT_EQ(640, app->GetWindow()->GetHeight());
    EXPECT_TRUE(app->GetDeviceManager()->GetParams().VSync);
}

TEST(App, ImGuiLayer_PlayModeCanSuspendMouseAndReceiveEscape)
{
    Testing::AppHarness::Shutdown();
    Application* app = Testing::AppHarness::Get();
    EP_REQUIRE(app != nullptr);
    EP_REQUIRE(app->GetImGuiLayer() != nullptr);

    app->GetImGuiLayer()->SetMouseInputEnabled(false);
    EXPECT_TRUE((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse) != 0);

    ImGuiIO& io = ImGui::GetIO();
    const bool previousWantCaptureKeyboard = io.WantCaptureKeyboard;
    io.WantCaptureKeyboard = true;

    app->GetImGuiLayer()->BlockEvents(true);
    KeyPressedEvent blockedEscape{ Key::Escape };
    app->GetImGuiLayer()->OnEvent(blockedEscape);
    EXPECT_TRUE(blockedEscape.Handled);

    app->GetImGuiLayer()->BlockEvents(false);
    KeyPressedEvent playModeEscape{ Key::Escape };
    app->GetImGuiLayer()->OnEvent(playModeEscape);
    EXPECT_TRUE(!playModeEscape.Handled);

    io.WantCaptureKeyboard = previousWantCaptureKeyboard;
    app->GetImGuiLayer()->SetMouseInputEnabled(true);
    EXPECT_TRUE((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse) == 0);
}

TEST(App, Application_WithoutImGui_UpdatesLayersWithoutUIRender)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);
    EXPECT_TRUE(app->GetImGuiLayer() == nullptr);

    const Ref<UITrackingLayer> layer = app->PushLayer<UITrackingLayer>();
    Testing::AppHarness::AdvanceFrames(3);
    EXPECT_TRUE(layer->UpdateCalled);
    EXPECT_TRUE(!layer->UIRenderCalled);
    EXPECT_TRUE(app->IsRunning());
}

TEST(App, Application_FullscreenUsesMonitorFramebufferSize)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .Fullscreen = true,
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);
    EXPECT_TRUE(app->GetWindow()->GetSpecification().Fullscreen);
    const auto [width, height] = app->GetWindow()->GetFramebufferSize();
    EXPECT_TRUE(width > 0);
    EXPECT_TRUE(height > 0);
    EXPECT_EQ(width, app->GetWindow()->GetWidth());
    EXPECT_EQ(height, app->GetWindow()->GetHeight());
}

TEST(App, Application_WindowResizePropagatesToLayers)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    Application* app = Testing::AppHarness::Get();
    const Ref<UITrackingLayer> layer = app->PushLayer<UITrackingLayer>();
    WindowResizeEvent event(1280, 720);
    app->OnEvent(event);

    EXPECT_TRUE(!event.Handled);
    EXPECT_EQ(1280, layer->ResizeWidth);
    EXPECT_EQ(720, layer->ResizeHeight);
}
