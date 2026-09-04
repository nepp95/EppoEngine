#pragma once

#include "Renderer/GpuProfiler.h"

namespace Eppo
{
    class VulkanGpuProfiler final : public GpuProfiler
    {
    public:
        VulkanGpuProfiler();
        ~VulkanGpuProfiler() override;

        auto
        BeginZone(const Ref<RenderCommandBuffer>& commandBuffer, const char* name, const char* function, const char* file, uint32_t line)
            -> void override;
        auto EndZone() -> void override;
        auto Collect(const Ref<RenderCommandBuffer>& commandBuffer) -> void override;

    private:
#if defined(TRACY_ENABLE)
        TracyVkCtx m_Context = nullptr;
        std::optional<tracy::VkCtxScope> m_Zone;
#endif
    };
}
