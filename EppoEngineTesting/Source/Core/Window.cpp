#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Core/Window.h"

using namespace Eppo;

// Graphical: needs a real display + GPU (suite CoreGraphical, label `graphical`, excluded
// from headless CI). The WM resize/fullscreen/creation-failure branches can't be driven
// from a test and stay uncovered; what is reachable is the spec echo, cursor mode, and the
// SetIcon decode-or-noop split.

TEST(CoreGraphical, Window_Boot_EchoesHarnessSpec)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .Title = "Window Spec Test",
        .Width = 800,
        .Height = 600,
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    if (!app)
        return;

    const Ref<Window>& window = app->GetWindow();
    EP_REQUIRE(window != nullptr);

    EXPECT_TRUE(window->GetNative() != nullptr);
    EXPECT_EQ(std::string("Window Spec Test"), window->GetSpecification().Title);
    EXPECT_EQ(800u, window->GetWidth());
    EXPECT_EQ(600u, window->GetHeight());

    const auto [framebufferWidth, framebufferHeight] = window->GetFramebufferSize();
    EXPECT_TRUE(framebufferWidth > 0);
    EXPECT_TRUE(framebufferHeight > 0);
}

TEST(CoreGraphical, Window_SetCursorMode_EveryModeDoesNotCrash)
{
    Application* app = Testing::AppHarness::Get();
    if (!app)
        return;

    const Ref<Window>& window = app->GetWindow();
    EP_REQUIRE(window != nullptr);

    window->SetCursorMode(CursorMode::Hidden);
    window->SetCursorMode(CursorMode::Disabled);
    window->SetCursorMode(CursorMode::Normal);
    SUCCEED();
}

TEST(CoreGraphical, Window_SetIcon_DecodableImageSucceeds)
{
    Application* app = Testing::AppHarness::Get();
    if (!app)
        return;

    const Ref<Window>& window = app->GetWindow();
    EP_REQUIRE(window != nullptr);

    // The runner chdirs to EppoEditor, so this editor resource resolves.
    window->SetIcon("Resources/Icons/Logo.png");
    SUCCEED();
}

TEST(CoreGraphical, Window_SetIcon_MissingImageIsNoOp)
{
    Application* app = Testing::AppHarness::Get();
    if (!app)
        return;

    const Ref<Window>& window = app->GetWindow();
    EP_REQUIRE(window != nullptr);

    window->SetIcon("Resources/Icons/DefinitelyNotAFile.png");
    SUCCEED();
}
