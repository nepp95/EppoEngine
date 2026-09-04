#include "pch.h"
#include "Renderer/DeviceManager.h"

#include "Core/Application.h"
#include "Platform/Vulkan/DeviceManagerVK.h"
#include "Renderer/GpuProfiler.h"

#if defined(EP_PLATFORM_WINDOWS)
    #include "Platform/DX12/DeviceManagerDX12.h"
#endif

namespace Eppo
{
    auto DeviceManager::Get() -> Ref<DeviceManager>
    {
        return Application::Get().GetDeviceManager();
    }

    auto DeviceManager::Create(const Ref<Window>& window, const DeviceParams& params) -> ScopedPtr<DeviceManager>
    {
        EP_ASSERT(params.API != RendererAPI::None, "No renderer api selected!");
#if !defined(EP_PLATFORM_WINDOWS)
        EP_ASSERT(params.API != RendererAPI::DX12, "DX12 renderer api selected on a non windows target!");
#endif
        EP_ASSERT(params.MaxFramesInFlight >= 2);

        switch (params.API)
        {
#if defined(EP_PLATFORM_WINDOWS)
            case RendererAPI::DX12:
            {
                Log::Info("Creating DirectX 12 device");
                return CreateScopedPtr<DeviceManagerDX12>(window, params);
            }
#endif

            case RendererAPI::Vulkan:
            {
                Log::Info("Creating Vulkan device");
                return CreateScopedPtr<DeviceManagerVK>(window, params);
            }

            default:
                break;
        }

        EP_ASSERT(false);
        return nullptr;
    }

    auto DeviceManager::WaitIdle() const -> bool
    {
        return GetDevice()->waitForIdle();
    }

    auto DeviceManager::InitRenderer() -> void
    {
        // Shader binding layouts need the published Renderer's descriptor manager during Init().
        m_Renderer = CreateScopedPtr<Renderer>();
        m_Renderer->Init();

        GpuProfiler::Init();
    }

    DeviceManager::DeviceManager(const Ref<Window>& window, DeviceParams params)
        : m_Params(std::move(params)), m_Window(window)
    {}
}
