#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/TestContext.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"
#include "Renderer/Swapchain.h"

#include <GLFW/glfw3.h>

using namespace Eppo;

TEST(Renderer, Swapchain_ResizePreservesFrameCycleUntilNextResize)
{
    Testing::TestContext context;
    ASSERT_TRUE(context.IsAvailable());

    const auto* application = Testing::AppHarness::Get();
    const auto& swapchain = DeviceManager::Get()->GetSwapchain();
    auto* window = application->GetWindow()->GetNative();
    int originalWidth = 0;
    int originalHeight = 0;
    glfwGetWindowSize(window, &originalWidth, &originalHeight);

    const auto image = Image::Create(ImageSpecification{ .ImageFormat = nvrhi::Format::RGBA8_UNORM, .Width = 32, .Height = 32 });
    context.AdvanceFrames(swapchain->GetMaxFramesInFlight() + 1, [&](float) -> void
    {
        DeviceManager::Get()->GetRenderer()->CompositeToSwapchain(image);
    });
    glfwSetWindowSize(window, originalWidth - 64, originalHeight - 32);
    context.AdvanceFrames(1);

    const auto [width, height] = application->GetWindow()->GetFramebufferSize();
    EXPECT_EQ(width, swapchain->GetWidth());
    EXPECT_EQ(height, swapchain->GetHeight());
    const WeakRef<Framebuffer> framebuffer = swapchain->GetCurrentSwapchainImage().Framebuffer;
    auto expectedFrameIndex = swapchain->GetCurrentFrameIndex();
    const auto frameCount = swapchain->GetMaxFramesInFlight() * 3;
    for (uint32_t frame = 0; frame < frameCount; frame++)
    {
        context.AdvanceFrames(1);
        expectedFrameIndex = (expectedFrameIndex + 1) % swapchain->GetMaxFramesInFlight();
        EXPECT_EQ(expectedFrameIndex, swapchain->GetCurrentFrameIndex());
        EXPECT_FALSE(framebuffer.expired());
        EXPECT_LT(swapchain->GetCurrentBackBufferIndex(), swapchain->GetImageCount());
    }

    glfwSetWindowSize(window, originalWidth, originalHeight);
    context.AdvanceFrames(1);
    EXPECT_TRUE(framebuffer.expired());
}
