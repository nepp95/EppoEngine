#include "pch.h"
#include "Platform/DX12/DX12Swapchain.h"

#include "Platform/DX12/DeviceManagerDX12.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Eppo
{
    namespace
    {
        auto NvrhiFormatToDxgi(const nvrhi::Format format) -> DXGI_FORMAT
        {
            switch (format)
            {
                case nvrhi::Format::RGBA8_UNORM:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                case nvrhi::Format::BGRA8_UNORM:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                case nvrhi::Format::SRGBA8_UNORM:
                    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case nvrhi::Format::SBGRA8_UNORM:
                    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                default:
                    return DXGI_FORMAT_UNKNOWN;
            }
        }
    }

    DX12Swapchain::DX12Swapchain(GLFWwindow* window)
        : Swapchain(window)
    {
        m_WindowHandle = glfwGetWin32Window(window);
        EP_ASSERT(m_WindowHandle);
    }

    DX12Swapchain::~DX12Swapchain()
    {
        // Viewport swapchains are destroyed while the device is still alive
        DeviceManager::Get()->WaitIdle();
        m_Images.clear();
    }

    auto DX12Swapchain::BeginFrame() -> bool
    {
        EP_PROFILE_FN("Swapchain::BeginFrame");
        EP_ASSERT(!m_FrameActive, "BeginFrame was called while a swapchain frame is already active!");

        const auto [width, height] = GetWindowFramebufferSize();
        if (width == 0 || height == 0)
            return false;

        if (width != m_Width || height != m_Height)
            m_ResizePending = true;

        if (m_ResizePending)
            Resize();

        const auto& dm = DeviceManager::Get();
        auto& frame = m_FrameSyncData.at(m_CurrentFrameIndex);
        if (frame.InFlight)
        {
            dm->GetDevice()->waitEventQuery(frame.CompletionQuery);
            dm->GetDevice()->resetEventQuery(frame.CompletionQuery);
            frame.InFlight = false;
        }

        m_SwapchainImageIndex = m_Swapchain->GetCurrentBackBufferIndex();
        m_FrameActive = true;
        return true;
    }

    auto DX12Swapchain::Present() -> bool
    {
        EP_PROFILE_FN("Swapchain::Present");
        EP_ASSERT(m_FrameActive, "Present was called without an active swapchain frame!");

        const auto& dm = std::static_pointer_cast<DeviceManagerDX12>(DeviceManager::Get());
        nvrhi::d3d12::IDevice* dxNvrhiDevice = dm->GetDevice()->getNativeObject(nvrhi::ObjectTypes::Nvrhi_D3D12_Device);
        const DeviceParams& params = dm->GetParams();

        const UINT syncInterval = params.VSync ? 1 : 0;
        DXGI_SWAP_CHAIN_DESC1 desc{};
        DX_CHECK(m_Swapchain->GetDesc1(&desc), "Failed to get swapchain description!");
        const UINT presentFlags = !params.VSync && (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0 ? DXGI_PRESENT_ALLOW_TEARING : 0;

        const HRESULT result = m_Swapchain->Present(syncInterval, presentFlags);

        dxNvrhiDevice->executeCommandLists(nullptr, 0);

        auto& frame = m_FrameSyncData.at(m_CurrentFrameIndex);
        dm->GetDevice()->setEventQuery(frame.CompletionQuery, nvrhi::CommandQueue::Graphics);
        frame.InFlight = true;

        m_FrameActive = false;
        m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % m_MaxFramesInFlight;

        if (SUCCEEDED(result))
            return true;

        Log::Error(LogSource::DX12, "Failed to present a swapchain image: HRESULT 0x{:08X}", static_cast<uint32_t>(result));
        return false;
    }

    auto DX12Swapchain::CreateSwapchain(uint32_t width, uint32_t height) -> void
    {
        const auto& dm = std::static_pointer_cast<DeviceManagerDX12>(DeviceManager::Get());
        const DeviceParams& params = dm->GetParams();

        if (width == 0 || height == 0)
        {
            const auto [width, height] = GetWindowFramebufferSize();
            m_Width = width;
            m_Height = height;
        }
        else
        {
            m_Width = width;
            m_Height = height;
        }

        m_Format = NvrhiFormatToDxgi(params.SwapchainFormat);
        EP_ASSERT(m_Format != DXGI_FORMAT_UNKNOWN);

        if (m_Swapchain)
        {
            m_Images.clear();

            DXGI_SWAP_CHAIN_DESC1 desc{};
            DX_CHECK(m_Swapchain->GetDesc1(&desc), "Failed to get swapchain description!");
            DX_CHECK(
                m_Swapchain->ResizeBuffers(desc.BufferCount, m_Width, m_Height, DXGI_FORMAT_UNKNOWN, desc.Flags),
                "Failed to resize swapchain buffers!"
            );
        }
        else
        {
            BOOL allowTearing = FALSE;
            if (!params.VSync)
                dm->GetFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));

            const DXGI_SWAP_CHAIN_DESC1 desc{
                .Width = m_Width,
                .Height = m_Height,
                .Format = m_Format,
                .Stereo = FALSE,
                .SampleDesc = { .Count = 1, .Quality = 0 },
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = std::max(2u, params.MaxFramesInFlight),
                .Scaling = DXGI_SCALING_STRETCH,
                .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
                .Flags = allowTearing == TRUE ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u,
            };

            ComPtr<IDXGISwapChain1> swapchain;
            DX_CHECK(
                dm->GetFactory()->CreateSwapChainForHwnd(dm->GetGraphicsQueue(), m_WindowHandle, &desc, nullptr, nullptr, &swapchain),
                "Failed to create IDXGISwapChain!"
            );
            EP_ASSERT(swapchain);
            DX_CHECK(swapchain.As(&m_Swapchain), "Failed to query IDXGISwapChain4!");

            // Fullscreen transitions are owned by the window, not DXGI
            DX_CHECK(dm->GetFactory()->MakeWindowAssociation(m_WindowHandle, DXGI_MWA_NO_ALT_ENTER), "Failed to disable DXGI Alt+Enter!");
        }

        DXGI_SWAP_CHAIN_DESC1 scDesc{};
        DX_CHECK(m_Swapchain->GetDesc1(&scDesc), "Failed to get swapchain description!");
        m_MaxFramesInFlight = std::min(params.MaxFramesInFlight, scDesc.BufferCount);

        if (m_FrameSyncData.size() != m_MaxFramesInFlight)
        {
            m_FrameSyncData.clear();
            m_FrameSyncData.resize(m_MaxFramesInFlight);

            for (auto& frame : m_FrameSyncData)
            {
                frame.CompletionQuery = dm->GetDevice()->createEventQuery();
                EP_ASSERT(frame.CompletionQuery != nullptr, "Failed to create frame completion query.");
                frame.InFlight = false;
            }
        }
        else
        {
            for (auto& frame : m_FrameSyncData)
            {
                if (frame.InFlight)
                    dm->GetDevice()->resetEventQuery(frame.CompletionQuery);
                frame.InFlight = false;
            }
        }

        m_CurrentFrameIndex = 0;
        m_FrameActive = false;

        for (uint32_t i = 0; i < scDesc.BufferCount; i++)
        {
            ComPtr<ID3D12Resource> backBuffer;
            DX_CHECK(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)), "Failed to get swapchain back buffer!");

            auto& image = m_Images.emplace_back();
            image.NativeImage = backBuffer.Get();

            ImageSpecification imageSpec{
                .ImageFormat = params.SwapchainFormat,
                .Width = m_Width,
                .Height = m_Height,
                .IsRenderTarget = true,
                .InitialState = nvrhi::ResourceStates::Present,
                .DebugName = std::format("Swapchain Image {}", i),
            };

            FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .SwapchainTarget = true,
                .SwapchainImage = Image::Create(imageSpec, image.NativeImage),
                .DebugName = std::format("Swapchain Framebuffer {}", i),
            };

            image.Framebuffer = CreateRef<Framebuffer>(framebufferSpec);
        }

        m_SwapchainImageIndex = m_Swapchain->GetCurrentBackBufferIndex();
    }
}
