#pragma once

#include "Renderer/GpuProfiler.h"

namespace Eppo
{
    class VulkanGpuProfiler final : public GpuProfiler
    {
    public:
        VulkanGpuProfiler();
        ~VulkanGpuProfiler() override;

        auto Collect(const Ref<RenderCommandBuffer>& commandBuffer) -> void override;
        [[nodiscard]] auto GetNativeContext() const -> void* override;

    private:
#if defined(TRACY_ENABLE)
        TracyVkCtx m_Context = nullptr;
#endif
    };
}
