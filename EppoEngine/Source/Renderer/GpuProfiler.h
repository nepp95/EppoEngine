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

    #include <optional>
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

        virtual auto
        BeginZone(const Ref<RenderCommandBuffer>& commandBuffer, const char* name, const char* function, const char* file, uint32_t line)
            -> void = 0;
        virtual auto EndZone() -> void = 0;
        virtual auto Collect(const Ref<RenderCommandBuffer>& commandBuffer) -> void = 0;

    protected:
        GpuProfiler() = default;

    private:
        static ScopedPtr<GpuProfiler> s_Instance;
    };

#if defined(TRACY_ENABLE)
    #define EP_GPU_ZONE(commandBuffer, name) GpuProfiler::Get()->BeginZone(commandBuffer, name, TracyFunction, TracyFile, TracyLine);
    #define EP_GPU_ZONE_END() GpuProfiler::Get()->EndZone();
    #define EP_GPU_COLLECT(commandBuffer) GpuProfiler::Get()->Collect(commandBuffer)
#else
    #define EP_GPU_ZONE(commandBuffer, name)
    #define EP_GPU_ZONE_END()
    #define EP_GPU_COLLECT(commandBuffer)
#endif
}
