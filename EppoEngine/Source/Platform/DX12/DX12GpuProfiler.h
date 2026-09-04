#pragma once

#include "Renderer/GpuProfiler.h"

namespace Eppo
{
    class DX12GpuProfiler final : public GpuProfiler
    {
    public:
        DX12GpuProfiler();
        ~DX12GpuProfiler() override;

        auto
        BeginZone(const Ref<RenderCommandBuffer>& commandBuffer, const char* name, const char* function, const char* file, uint32_t line)
            -> void override;
        auto EndZone() -> void override;
        auto Collect(const Ref<RenderCommandBuffer>& commandBuffer) -> void override;

    private:
#if defined(TRACY_ENABLE)
        TracyD3D12Ctx m_Context = nullptr;
        std::optional<tracy::D3D12ZoneScope> m_Zone;
#endif
    };
}
