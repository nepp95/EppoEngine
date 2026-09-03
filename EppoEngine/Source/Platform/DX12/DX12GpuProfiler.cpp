#include "pch.h"
#include "Platform/DX12/DX12GpuProfiler.h"

#include "Platform/DX12/DeviceManagerDX12.h"

namespace Eppo
{
    DX12GpuProfiler::DX12GpuProfiler()
    {
#if defined(TRACY_ENABLE)
        const auto dm = std::static_pointer_cast<DeviceManagerDX12>(DeviceManager::Get());
        m_Context = TracyD3D12Context(dm->GetDxDevice(), dm->GetGraphicsQueue());
#endif
    }

    DX12GpuProfiler::~DX12GpuProfiler()
    {
#if defined(TRACY_ENABLE)
        if (m_Context)
            TracyD3D12Destroy(m_Context);
#endif
    }

    auto DX12GpuProfiler::Collect([[maybe_unused]] const Ref<RenderCommandBuffer>& commandBuffer) -> void
    {
#if defined(TRACY_ENABLE)
        TracyD3D12Collect(m_Context);
#endif
    }

    auto DX12GpuProfiler::GetNativeContext() const -> void*
    {
#if defined(TRACY_ENABLE)
        return m_Context;
#else
        return nullptr;
#endif
    }
}
