#pragma once

#include <nvrhi/nvrhi.h>

#if defined(TRACY_ENABLE)
    #if defined(EP_PLATFORM_WINDOWS) && !defined(NOMINMAX)
        #define NOMINMAX
    #endif

    #include <vulkan/vulkan.h>

    #include <tracy/TracyVulkan.hpp>
    #if defined(EP_PLATFORM_WINDOWS)
        #include <tracy/TracyD3D12.hpp>
    #endif
#endif

namespace Eppo
{
    class RenderCommandBuffer;

    class GpuProfiler
    {
    public:
        GpuProfiler(const GpuProfiler&) = delete;
        GpuProfiler(GpuProfiler&&) = delete;
        auto operator=(const GpuProfiler&) -> GpuProfiler& = delete;
        auto operator=(GpuProfiler&&) -> GpuProfiler& = delete;
        virtual ~GpuProfiler() = default;

        static auto Init() -> void;
        static auto Shutdown() -> void;
        [[nodiscard]] static auto Get() -> GpuProfiler*;

        virtual auto Collect(const Ref<RenderCommandBuffer>& commandBuffer) -> void = 0;
        [[nodiscard]] virtual auto GetNativeContext() const -> void* = 0;

    protected:
        GpuProfiler() = default;

    private:
        static ScopedPtr<GpuProfiler> s_Instance;
    };

    // TracyVkZone is an RAII/source-location macro, so it must expand at the call site rather than behind the interface.
#if defined(TRACY_ENABLE)
    #define EP_GPU_ZONE(commandBuffer, name)                                                                                               \
        TracyVkZone(                                                                                                                       \
            static_cast<TracyVkCtx>(GpuProfiler::Get()->GetNativeContext()),                                                               \
            static_cast<VkCommandBuffer>((commandBuffer)->GetCommandList()->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer)), name   \
        )
    #define EP_GPU_COLLECT(commandBuffer) GpuProfiler::Get()->Collect(commandBuffer)
#else
    #define EP_GPU_ZONE(commandBuffer, name)
    #define EP_GPU_COLLECT(commandBuffer)
#endif
}
