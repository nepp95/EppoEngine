#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/RenderCommandBuffer.h"

using namespace Eppo;

struct RenderCommandBufferTimingState
{
    std::vector<uint32_t> FrameIndices;
    bool ReusedFrameSlot = false;
    float TimeAfterReuse = 0.0f;
    float NamedTimeAfterReuse = 0.0f;
};

class RenderCommandBufferTimingLayer : public Layer
{
public:
    explicit RenderCommandBufferTimingLayer(Ref<RenderCommandBufferTimingState> state)
        : m_State(std::move(state))
    {}

    auto OnUpdate(float) -> void override
    {
        const uint32_t frameIndex = DeviceManager::Get()->GetCurrentFrameIndex();
        const bool frameSlotWasUsed = std::ranges::find(m_State->FrameIndices, frameIndex) != m_State->FrameIndices.end();

        m_RenderCommandBuffer.Begin("Timing test");

        if (frameSlotWasUsed)
        {
            m_State->ReusedFrameSlot = true;
            m_State->TimeAfterReuse = m_RenderCommandBuffer.GetTime(frameIndex);
            m_State->NamedTimeAfterReuse = m_RenderCommandBuffer.GetTime("Pass", frameIndex);
        }

        m_RenderCommandBuffer.BeginTimerQuery("Pass");
        m_RenderCommandBuffer.EndTimerQuery("Pass");
        m_RenderCommandBuffer.End();
        m_RenderCommandBuffer.Submit();

        m_State->FrameIndices.emplace_back(frameIndex);
    }

private:
    Ref<RenderCommandBufferTimingState> m_State;
    RenderCommandBuffer m_RenderCommandBuffer;
};

TEST(Renderer, RenderCommandBuffer_AllocatesTimingDataForEveryFrameInFlight)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    const RenderCommandBuffer renderCommandBuffer;
    const uint32_t maxFramesInFlight = DeviceManager::Get()->GetMaxFramesInFlight();
    EP_REQUIRE(maxFramesInFlight > 0);
    const uint32_t frameIndex = maxFramesInFlight - 1;

    EXPECT_EQ(0.0f, renderCommandBuffer.GetTime(frameIndex));
    EXPECT_EQ(0.0f, renderCommandBuffer.GetTimeMs(frameIndex));
    EXPECT_EQ(0.0f, renderCommandBuffer.GetTime(maxFramesInFlight));
    EXPECT_EQ(0.0f, renderCommandBuffer.GetTime("Unrecorded", maxFramesInFlight));
}

TEST(Renderer, RenderCommandBuffer_BeginEndSubmitCompletes)
{
    if (!Testing::AppHarness::IsAvailable())
        return;

    RenderCommandBuffer renderCommandBuffer;
    const uint32_t frameIndex = DeviceManager::Get()->GetCurrentFrameIndex();

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
    const uint32_t frameIndex = DeviceManager::Get()->GetCurrentFrameIndex();

    renderCommandBuffer.Begin();
    renderCommandBuffer.BeginTimerQuery("Pass");
    renderCommandBuffer.EndTimerQuery("Pass");
    renderCommandBuffer.End();
    renderCommandBuffer.Submit();

    EXPECT_TRUE(renderCommandBuffer.GetTime("Pass", frameIndex) >= 0.0f);
}

TEST(Renderer, RenderCommandBuffer_TimingIsReadableAfterFrameSlotReuse)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    const uint32_t maxFramesInFlight = app->GetDeviceManager()->GetMaxFramesInFlight();
    EP_REQUIRE(maxFramesInFlight > 0);

    const auto state = CreateRef<RenderCommandBufferTimingState>();
    app->PushLayer<RenderCommandBufferTimingLayer>(state);
    Testing::AppHarness::AdvanceFrames(maxFramesInFlight + 1);

    EXPECT_TRUE(state->ReusedFrameSlot);
    EXPECT_TRUE(state->TimeAfterReuse >= 0.0f);
    EXPECT_TRUE(state->NamedTimeAfterReuse >= 0.0f);
}
