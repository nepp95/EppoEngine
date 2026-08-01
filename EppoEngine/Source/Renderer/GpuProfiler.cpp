#include "pch.h"
#include "Renderer/GpuProfiler.h"

#include "Platform/Vulkan/VulkanGpuProfiler.h"

namespace Eppo
{
    ScopedPtr<GpuProfiler> GpuProfiler::s_Instance = nullptr;

    auto GpuProfiler::Init() -> void
    {
        EP_ASSERT(!s_Instance, "GpuProfiler is already initialized!");
        // Vulkan is the only backend today; a D3D12 profiler would branch here on the active RendererAPI.
        s_Instance = CreateScopedPtr<VulkanGpuProfiler>();
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
