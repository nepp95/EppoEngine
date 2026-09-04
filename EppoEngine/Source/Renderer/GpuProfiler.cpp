#include "pch.h"
#include "Renderer/GpuProfiler.h"

#include "Platform/Vulkan/VulkanGpuProfiler.h"
#include "Renderer/DeviceManager.h"

#if defined(EP_PLATFORM_WINDOWS)
    #include "Platform/DX12/DX12GpuProfiler.h"
#endif

namespace Eppo
{
    ScopedPtr<GpuProfiler> GpuProfiler::s_Instance = nullptr;

    auto GpuProfiler::Init() -> void
    {
        EP_ASSERT(!s_Instance, "GpuProfiler is already initialized!");
        switch (DeviceManager::Get()->GetParams().API)
        {
#if defined(EP_PLATFORM_WINDOWS)
            case RendererAPI::DX12:
                s_Instance = CreateScopedPtr<DX12GpuProfiler>();
                break;
#endif

            case RendererAPI::Vulkan:
                s_Instance = CreateScopedPtr<VulkanGpuProfiler>();
                break;

            default:
                EP_ASSERT(false, "No renderer api selected!");
                break;
        }
    }

    auto GpuProfiler::Shutdown() -> void
    {
        s_Instance.reset();
    }

    auto GpuProfiler::Get() -> GpuProfiler*
    {
        EP_ASSERT(s_Instance != nullptr, "GpuProfiler::Get() called before Init() or after Shutdown()!");
        return s_Instance.get();
    }
}
