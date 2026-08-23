#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Event/KeyEvent.h"
#include "ImGui/ImGuiLayer.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

#include <imgui.h>

using namespace Eppo;

// Headless: pure argument accessor, no Application boot. The out-of-bounds path is an
// assert-guarded dead return, so only valid indices are pinned.
TEST(Core, CommandLineArgs_ValidIndex_ReturnsMatchingArgv)
{
    char arg0[] = "EppoEditor";
    char arg1[] = "--project";
    char arg2[] = "Sandbox.eproj";
    char* argv[] = { arg0, arg1, arg2 };
    const CommandLineArgs args(3, argv);

    EXPECT_EQ(arg0, args[0]);
    EXPECT_EQ(arg1, args[1]);
    EXPECT_EQ(arg2, args[2]);
}

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

struct ThreadPoolTeardownState
{
    bool Detached = false;
    bool CompletionCalled = false;
    bool CompletionBeforeDetach = false;
};

class ThreadPoolTeardownLayer : public Layer
{
public:
    explicit ThreadPoolTeardownLayer(Ref<ThreadPoolTeardownState> state)
        : m_State(std::move(state))
    {}

    auto OnAttach() -> void override
    {
        Application::Get().GetThreadPool()->QueueTask(
            "Teardown order",
            []() -> void
            {
            },
            [state = m_State](TaskStatus) -> void
            {
                state->CompletionCalled = true;
                state->CompletionBeforeDetach = !state->Detached;
            }
        );
    }

    auto OnDetach() -> void override { m_State->Detached = true; }

private:
    Ref<ThreadPoolTeardownState> m_State;
};

struct RenderCommandTrackingState
{
    bool UpdateCompleted = false;
    bool UICompleted = false;
    bool CommandExecuted = false;
    bool ExecutedAfterUpdate = false;
    bool ExecutedAfterUI = false;
};

class RenderCommandTrackingLayer : public Layer
{
public:
    explicit RenderCommandTrackingLayer(Ref<RenderCommandTrackingState> state)
        : m_State(std::move(state))
    {}

    auto OnUpdate(float) -> void override
    {
        m_State->UpdateCompleted = true;
        Renderer::Submit(
            [state = m_State]() -> void
            {
                state->CommandExecuted = true;
                state->ExecutedAfterUpdate = state->UpdateCompleted;
                state->ExecutedAfterUI = state->UICompleted;
            }
        );
    }

    auto OnUIRender() -> void override { m_State->UICompleted = true; }

private:
    Ref<RenderCommandTrackingState> m_State;
};

struct FrameIndexTrackingState
{
    std::vector<uint32_t> FrameIndices;
    std::vector<uint32_t> BackBufferIndices;
};

class FrameIndexTrackingLayer : public Layer
{
public:
    explicit FrameIndexTrackingLayer(Ref<FrameIndexTrackingState> state)
        : m_State(std::move(state))
    {}

    auto OnUpdate(float) -> void override
    {
        const auto& deviceManager = DeviceManager::Get();
        m_State->FrameIndices.emplace_back(deviceManager->GetCurrentFrameIndex());
        m_State->BackBufferIndices.emplace_back(deviceManager->GetCurrentBackBufferIndex());
    }

private:
    Ref<FrameIndexTrackingState> m_State;
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

TEST(App, Application_ShutdownFlushesTaskCompletionsBeforeDetachingLayers)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    const auto state = CreateRef<ThreadPoolTeardownState>();
    app->PushLayer<ThreadPoolTeardownLayer>(state);

    Testing::AppHarness::Shutdown();

    EXPECT_TRUE(state->CompletionCalled);
    EXPECT_TRUE(state->CompletionBeforeDetach);
    EXPECT_TRUE(state->Detached);
}

TEST(App, Application_StepFrame_ExecutesSubmittedRenderCommandsAfterUI)
{
    Testing::AppHarness::Shutdown();
    Application* app = Testing::AppHarness::Get();
    EP_REQUIRE(app != nullptr);

    const auto state = CreateRef<RenderCommandTrackingState>();
    app->PushLayer<RenderCommandTrackingLayer>(state);

    Testing::AppHarness::AdvanceFrames(1);

    EXPECT_TRUE(state->UpdateCompleted);
    EXPECT_TRUE(state->UICompleted);
    EXPECT_TRUE(state->CommandExecuted);
    EXPECT_TRUE(state->ExecutedAfterUpdate);
    EXPECT_TRUE(state->ExecutedAfterUI);
}

TEST(App, Application_StepFrame_ExecutesSubmittedRenderCommandsWithoutImGui)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    const auto state = CreateRef<RenderCommandTrackingState>();
    app->PushLayer<RenderCommandTrackingLayer>(state);

    Testing::AppHarness::AdvanceFrames(1);

    EXPECT_TRUE(state->UpdateCompleted);
    EXPECT_FALSE(state->UICompleted);
    EXPECT_TRUE(state->CommandExecuted);
    EXPECT_TRUE(state->ExecutedAfterUpdate);
    EXPECT_FALSE(state->ExecutedAfterUI);
}

TEST(App, DeviceManager_FrameAndBackBufferCounts_AreValidIndependentRanges)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    const auto& deviceManager = app->GetDeviceManager();
    const uint32_t backBufferCount = deviceManager->GetBackBufferCount();
    const uint32_t expectedFramesInFlight = std::min(deviceManager->GetParams().MaxFramesInFlight, backBufferCount);

    EXPECT_GE(backBufferCount, 2u);
    EXPECT_EQ(expectedFramesInFlight, deviceManager->GetMaxFramesInFlight());
    EXPECT_LT(deviceManager->GetCurrentFrameIndex(), deviceManager->GetMaxFramesInFlight());
    EXPECT_LT(deviceManager->GetCurrentBackBufferIndex(), backBufferCount);
}

TEST(App, DeviceManager_CurrentFrameIndex_RotatesAcrossFramesInFlight)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    const auto& deviceManager = app->GetDeviceManager();
    const uint32_t maxFramesInFlight = deviceManager->GetMaxFramesInFlight();
    EP_REQUIRE(maxFramesInFlight > 0);

    const auto state = CreateRef<FrameIndexTrackingState>();
    app->PushLayer<FrameIndexTrackingLayer>(state);
    Testing::AppHarness::AdvanceFrames(maxFramesInFlight * 2 + 1);

    EP_REQUIRE_EQ(maxFramesInFlight * 2 + 1, state->FrameIndices.size());
    EXPECT_EQ(state->FrameIndices.size(), state->BackBufferIndices.size());

    const uint32_t firstFrameIndex = state->FrameIndices.front();
    for (size_t i = 0; i < state->FrameIndices.size(); i++)
    {
        EXPECT_EQ((firstFrameIndex + i) % maxFramesInFlight, state->FrameIndices.at(i));
        EXPECT_LT(state->BackBufferIndices.at(i), deviceManager->GetBackBufferCount());
    }
}

TEST(App, Application_Shutdown_ReleasesPendingRenderCommandCaptures)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    auto capturedState = CreateRef<uint32_t>(42);
    const WeakRef<uint32_t> weakState = capturedState;

    Renderer::Submit([capturedState]() -> void {});
    capturedState.reset();

    EXPECT_FALSE(weakState.expired());

    Testing::AppHarness::Shutdown();

    EXPECT_TRUE(weakState.expired());
}
