#include "TestSupport/EppoTest.h"
#include "TestSupport/AppHarness.h"
#include "TestSupport/TestContext.h"

#include "Platform/Vulkan/VulkanGpuProfiler.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/GpuProfiler.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/Renderer.h"

#if defined(EP_PLATFORM_WINDOWS)
    #include "Platform/DX12/DX12GpuProfiler.h"
    #include "Platform/DX12/DeviceManagerDX12.h"
    #include <d3d12sdklayers.h>
#endif

using namespace Eppo;

TEST(Renderer, GpuProfiler_UsesSelectedBackend)
{
    ASSERT_TRUE(Testing::AppHarness::IsAvailable());

    const auto* profiler = GpuProfiler::Get();
    ASSERT_NE(nullptr, profiler);
    switch (DeviceManager::Get()->GetParams().API)
    {
        case RendererAPI::Vulkan:
            EXPECT_NE(nullptr, dynamic_cast<const VulkanGpuProfiler*>(profiler));
            break;
#if defined(EP_PLATFORM_WINDOWS)
        case RendererAPI::DX12:
            EXPECT_NE(nullptr, dynamic_cast<const DX12GpuProfiler*>(profiler));
            break;
#endif
        default:
            FAIL() << "No supported GPU profiler backend selected.";
    }
}

TEST(Renderer, GpuProfiler_NestedZonesSurviveFrameReuseAndCollection)
{
    Testing::TestContext context;
    ASSERT_TRUE(context.IsAvailable());

    const auto deviceManager = DeviceManager::Get();
#if defined(EP_PLATFORM_WINDOWS)
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (s_EnableValidationLayers && deviceManager->GetParams().API == RendererAPI::DX12)
    {
        const auto dxDeviceManager = std::static_pointer_cast<DeviceManagerDX12>(deviceManager);
        ASSERT_TRUE(SUCCEEDED(dxDeviceManager->GetDxDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue))));
    }
    const auto firstMessage = infoQueue ? infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter() : 0;
#endif

    const auto commandBuffer = CreateRef<RenderCommandBuffer>();
    const auto frameCount = deviceManager->GetMaxFramesInFlight() * 4;
    uint32_t submittedFrames = 0;
    context.AdvanceFrames(frameCount, [&](float) -> void
    {
        Renderer::Submit([&]() -> void
        {
            commandBuffer->Begin();
            {
                EP_GPU_ZONE(commandBuffer, "ProfilerOuterZone")
                {
                    EP_GPU_ZONE(commandBuffer, "ProfilerInnerZone")
                }
            }
            EP_GPU_COLLECT(commandBuffer);
            commandBuffer->End();
            commandBuffer->Submit();
            submittedFrames++;
        });
    });

    ASSERT_TRUE(deviceManager->WaitIdle());
    EXPECT_EQ(frameCount, submittedFrames);
#if defined(EP_PLATFORM_WINDOWS)
    if (infoQueue)
    {
        for (auto index = firstMessage; index < infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter(); index++)
        {
            SIZE_T size = 0;
            ASSERT_TRUE(SUCCEEDED(infoQueue->GetMessage(index, nullptr, &size)));
            std::vector<uint8_t> storage(size);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            ASSERT_TRUE(SUCCEEDED(infoQueue->GetMessage(index, message, &size)));
            EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
        }
    }
#endif
}
