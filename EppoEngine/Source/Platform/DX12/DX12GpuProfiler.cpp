#include "pch.h"
#include "Platform/DX12/DX12GpuProfiler.h"

#include "Platform/DX12/DeviceManagerDX12.h"
#include "Renderer/RenderCommandBuffer.h"

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

    auto DX12GpuProfiler::BeginZone(
        [[maybe_unused]] const Ref<RenderCommandBuffer>& commandBuffer, [[maybe_unused]] const char* name,
        [[maybe_unused]] const char* function, [[maybe_unused]] const char* file, [[maybe_unused]] const uint32_t line
    ) -> void
    {
#if defined(TRACY_ENABLE)
        auto* cmd = static_cast<ID3D12GraphicsCommandList*>(
            commandBuffer->GetCommandList()->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList)
        );
        m_Zone.emplace(m_Context, line, file, strlen(file), function, strlen(function), name, strlen(name), cmd, true);
#endif
    }

    auto DX12GpuProfiler::EndZone() -> void
    {
#if defined(TRACY_ENABLE)
        m_Zone.reset();
#endif
    }

    auto DX12GpuProfiler::Collect([[maybe_unused]] const Ref<RenderCommandBuffer>& commandBuffer) -> void
    {
#if defined(TRACY_ENABLE)
        TracyD3D12Collect(m_Context);
#endif
    }

}
