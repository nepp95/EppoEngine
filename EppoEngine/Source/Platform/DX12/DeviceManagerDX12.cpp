#include "pch.h"
#include "Platform/DX12/DeviceManagerDX12.h"

#include "Platform/DX12/DX12Swapchain.h"
#include "Renderer/GpuProfiler.h"

namespace Eppo
{
    namespace
    {
        auto SelectHardwareAdapter(IDXGIFactory6* factory, IDXGIAdapter1** outAdapter) -> void
        {
            *outAdapter = nullptr;
            ComPtr<IDXGIAdapter1> adapter;

            auto probeAdapter = [&adapter](IDXGIAdapter1* candidate) -> bool
            {
                DXGI_ADAPTER_DESC1 desc;
                candidate->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    return false;

                return SUCCEEDED(D3D12CreateDevice(candidate, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr));
            };

            for (UINT i = 0;
                 SUCCEEDED(factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); ++i)
            {
                if (probeAdapter(adapter.Get()))
                    break;
            }

            if (adapter == nullptr)
            {
                for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i)
                {
                    if (probeAdapter(adapter.Get()))
                        break;
                }
            }

            *outAdapter = adapter.Detach();
        }
    }

    DeviceManagerDX12::DeviceManagerDX12(const Ref<Window>& window, const DeviceParams& params)
        : DeviceManager(window, params)
    {
        UINT dxgiFactoryFlags = 0;

        if (s_EnableValidationLayers)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }

        DX_CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)), "Failed to create IDXGIFactory6!");
        EP_ASSERT(m_Factory);

        ComPtr<IDXGIAdapter1> hardwareAdapter;
        SelectHardwareAdapter(m_Factory.Get(), &hardwareAdapter);
        EP_ASSERT(hardwareAdapter);

        DX_CHECK(
            D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_DxDevice)), "Failed to create ID3D12Device!"
        );
        EP_ASSERT(m_DxDevice);

        // Command queues
        D3D12_COMMAND_QUEUE_DESC queueDesc{
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        };
        DX_CHECK(m_DxDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue)), "Failed to create graphics command queue!");

        if (m_Params.EnableComputeQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            DX_CHECK(m_DxDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue)), "Failed to create compute command queue!");
        }

        if (m_Params.EnableTransferQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            DX_CHECK(m_DxDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_TransferQueue)), "Failed to create transfer command queue!");
        }

        CreateNvrhiDevice();
    }

    auto DeviceManagerDX12::Init() -> void
    {
        m_Swapchain = CreateSwapchain(m_Window->GetNative(), 0, 0);
    }

    auto DeviceManagerDX12::Shutdown() -> void
    {
        WaitIdle();
        GpuProfiler::Shutdown();

        m_Renderer = nullptr;
        m_Swapchain = nullptr;

        m_Device->runGarbageCollection();
        m_Device = nullptr;
        m_ValidationLayer = nullptr;
    }

    auto DeviceManagerDX12::CreateSwapchain(GLFWwindow* window, const uint32_t width, const uint32_t height) -> Ref<Swapchain>
    {
        Ref<DX12Swapchain> swapchain = CreateRef<DX12Swapchain>(window);
        swapchain->CreateSwapchain(width, height);
        return swapchain;
    }

    auto DeviceManagerDX12::GetDevice() const -> nvrhi::IDevice*
    {
        if (m_ValidationLayer)
            return m_ValidationLayer;

        return m_Device;
    }

    auto DeviceManagerDX12::CreateNvrhiDevice() -> void
    {
        const nvrhi::d3d12::DeviceDesc deviceDesc{
            .errorCB = &m_MessageCallback,
            .pDevice = m_DxDevice.Get(),
            .pGraphicsCommandQueue = m_GraphicsQueue.Get(),
            .pComputeCommandQueue = m_ComputeQueue.Get(),
            .pCopyCommandQueue = m_TransferQueue.Get(),
            .enableHeapDirectlyIndexed = true,
        };

        m_Device = nvrhi::d3d12::createDevice(deviceDesc);

        if (s_EnableValidationLayers)
            m_ValidationLayer = nvrhi::validation::createValidationLayer(m_Device);
    }
}
