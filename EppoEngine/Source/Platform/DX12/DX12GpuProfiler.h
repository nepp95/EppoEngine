#pragma once

#include "Renderer/GpuProfiler.h"

namespace Eppo
{
    class DX12GpuProfiler final : public GpuProfiler
    {
    public:
        DX12GpuProfiler();
        ~DX12GpuProfiler() override;

        auto Collect(const Ref<RenderCommandBuffer>& commandBuffer) -> void override;
        [[nodiscard]] auto GetNativeContext() const -> void* override;

    private:
#if defined(TRACY_ENABLE)
        TracyD3D12Ctx m_Context = nullptr;
#endif
    };
}
