#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/RenderCommandBuffer.h"

using namespace Eppo;

TEST(Renderer, RenderCommandBuffer_AllocatesTimingDataForEveryBackBuffer)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const RenderCommandBuffer renderCommandBuffer;
    const uint32_t frameIndex = DeviceManager::Get()->GetBackBufferCount() - 1;

    EXPECT_EQ(0.0f, renderCommandBuffer.GetTime(frameIndex));
    EXPECT_EQ(0.0f, renderCommandBuffer.GetTimeMs(frameIndex));
    EXPECT_EQ(0.0f, renderCommandBuffer.GetTime(frameIndex + 1));
    EXPECT_EQ(0.0f, renderCommandBuffer.GetTime("Unrecorded", frameIndex + 1));
}

TEST(Renderer, RenderCommandBuffer_BeginEndSubmitCompletes)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    RenderCommandBuffer renderCommandBuffer;
    const uint32_t frameIndex = DeviceManager::Get()->GetCurrentBackBufferIndex();

    renderCommandBuffer.Begin("Test");
    EXPECT_TRUE(renderCommandBuffer.GetCommandList());
    renderCommandBuffer.End();
    renderCommandBuffer.Submit();

    EXPECT_TRUE(renderCommandBuffer.GetTime(frameIndex) >= 0.0f);
}

TEST(Renderer, RenderCommandBuffer_NamedTimerIsReadableAfterSubmit)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    RenderCommandBuffer renderCommandBuffer;
    const uint32_t frameIndex = DeviceManager::Get()->GetCurrentBackBufferIndex();

    renderCommandBuffer.Begin();
    renderCommandBuffer.BeginTimerQuery("Pass");
    renderCommandBuffer.EndTimerQuery("Pass");
    renderCommandBuffer.End();
    renderCommandBuffer.Submit();

    EXPECT_TRUE(renderCommandBuffer.GetTime("Pass", frameIndex) >= 0.0f);
}
