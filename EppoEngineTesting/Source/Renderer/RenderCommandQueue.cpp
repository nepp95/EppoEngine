#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"

#include "Renderer/RenderCommandQueue.h"
#include "Renderer/Renderer.h"

using namespace Eppo;

TEST(Core, RenderCommandQueue_Execute_RunsCommandsInSubmissionOrder)
{
    RenderCommandQueue queue;
    std::vector<uint32_t> executionOrder;

    queue.AddCommand([&executionOrder]() -> void { executionOrder.emplace_back(1); });
    queue.AddCommand([&executionOrder]() -> void { executionOrder.emplace_back(2); });
    queue.AddCommand([&executionOrder]() -> void { executionOrder.emplace_back(3); });

    queue.Execute();

    const std::vector<uint32_t> expected{ 1, 2, 3 };
    EXPECT_EQ(expected, executionOrder);
}

TEST(Core, RenderCommandQueue_Execute_ExecutesEachCommandOnce)
{
    RenderCommandQueue queue;
    uint32_t executionCount = 0;

    queue.AddCommand([&executionCount]() -> void { executionCount++; });

    queue.Execute();
    queue.Execute();

    EXPECT_EQ(1u, executionCount);
}

TEST(Core, RenderCommandQueue_Clear_DiscardsPendingCommands)
{
    RenderCommandQueue queue;
    bool commandExecuted = false;

    queue.AddCommand([&commandExecuted]() -> void { commandExecuted = true; });
    queue.Clear();
    queue.Execute();

    EXPECT_FALSE(commandExecuted);
}

TEST(Core, RenderCommandQueue_CommandSubmittedDuringExecution_WaitsForNextBatch)
{
    RenderCommandQueue queue;
    std::vector<uint32_t> executionOrder;

    queue.AddCommand(
        [&queue, &executionOrder]() -> void
        {
            executionOrder.emplace_back(1);
            queue.AddCommand([&executionOrder]() -> void { executionOrder.emplace_back(2); });
        }
    );

    queue.Execute();
    EXPECT_EQ(std::vector<uint32_t>{ 1 }, executionOrder);

    queue.Execute();
    EXPECT_EQ((std::vector<uint32_t>{ 1, 2 }), executionOrder);
}

TEST(App, Renderer_Submit_QueuesUntilRenderCommandsAreExecuted)
{
    Testing::AppHarness::Shutdown();
    ApplicationParams params{
        .Args = CommandLineArgs(0, nullptr),
        .EnableImGui = false,
    };
    Application* app = Testing::AppHarness::Get(std::move(params));
    EP_REQUIRE(app != nullptr);

    bool commandExecuted = false;
    Renderer::Submit([&commandExecuted]() -> void { commandExecuted = true; });

    EXPECT_FALSE(commandExecuted);

    Renderer::ExecuteRenderCommands();

    EXPECT_TRUE(commandExecuted);
}
