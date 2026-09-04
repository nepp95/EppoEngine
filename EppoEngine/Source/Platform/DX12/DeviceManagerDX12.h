#pragma once

#include "Platform/DX12/DX12.h"
#include "Renderer/DeviceManager.h"

#include <nvrhi/d3d12.h>
#include <nvrhi/nvrhi.h>
#include <nvrhi/validation.h>

namespace Eppo
{
    class DeviceManagerDX12 : public DeviceManager
    {
    public:
        explicit DeviceManagerDX12(const Ref<Window>& window, const DeviceParams& params);
        ~DeviceManagerDX12() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto CreateSwapchain(GLFWwindow* window, uint32_t width, uint32_t height) -> Ref<Swapchain> override;

        [[nodiscard]] auto GetDevice() const -> nvrhi::IDevice* override;

        [[nodiscard]] auto GetDxDevice() const -> ID3D12Device* { return m_DxDevice.Get(); }
        [[nodiscard]] auto GetFactory() const -> IDXGIFactory6* { return m_Factory.Get(); }
        [[nodiscard]] auto GetGraphicsQueue() const -> ID3D12CommandQueue* { return m_GraphicsQueue.Get(); }

    private:
        auto CreateNvrhiDevice() -> void;

    private:
        nvrhi::d3d12::DeviceHandle m_Device = nullptr;
        nvrhi::DeviceHandle m_ValidationLayer = nullptr;

        ComPtr<ID3D12Device> m_DxDevice;
        ComPtr<IDXGIFactory6> m_Factory;
        ComPtr<ID3D12CommandQueue> m_ComputeQueue;
        ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
        ComPtr<ID3D12CommandQueue> m_TransferQueue;
    };
}
