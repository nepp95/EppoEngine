#pragma once

#include "Platform/DX12/DX12.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Swapchain.h"

namespace Eppo
{
    class DX12Swapchain : public Swapchain
    {
    public:
        explicit DX12Swapchain(GLFWwindow* window);
        ~DX12Swapchain() override;

        auto BeginFrame() -> bool override;
        auto Present() -> bool override;

        auto CreateSwapchain(uint32_t width = 0, uint32_t height = 0) -> void override;

    private:
        struct FrameSync
        {
            nvrhi::EventQueryHandle CompletionQuery = nullptr;
            bool InFlight = false;
        };

        std::vector<FrameSync> m_FrameSyncData;
        HWND m_WindowHandle = nullptr;
        ComPtr<IDXGISwapChain4> m_Swapchain;

        DXGI_FORMAT m_Format = DXGI_FORMAT_UNKNOWN;
    };
}
