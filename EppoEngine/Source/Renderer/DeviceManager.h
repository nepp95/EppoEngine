#pragma once

#include "Core/Window.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/Swapchain.h"

#include <nvrhi/nvrhi.h>

struct GLFWwindow;

namespace Eppo
{
    class DeviceManagerVK;
    class DeviceManagerDX12;

#if !defined(EP_DIST)
    constexpr bool s_EnableValidationLayers = true;
#else
    constexpr bool s_EnableValidationLayers = false;
#endif

    enum class RendererAPI
    {
        None,
        DX12,
        Vulkan,
    };

    struct NvrhiMessageCallback final : nvrhi::IMessageCallback
    {
        auto message(const nvrhi::MessageSeverity severity, const char* messageText) -> void override
        {
            switch (severity)
            {
                case nvrhi::MessageSeverity::Info:
                {
                    Log::Info("{}", messageText);
                    break;
                }

                case nvrhi::MessageSeverity::Warning:
                {
                    Log::Warn("{}", messageText);
                    break;
                }

                case nvrhi::MessageSeverity::Error:
                {
                    Log::Error("{}", messageText);
                    break;
                }

                case nvrhi::MessageSeverity::Fatal:
                {
                    Log::Error("{}", messageText);
                    EP_ASSERT(false, messageText);
                    break;
                }
            }
        }
    };

    struct DeviceParams
    {
        RendererAPI API = RendererAPI::Vulkan;

        nvrhi::Format SwapchainFormat = nvrhi::Format::RGBA8_UNORM;
        uint32_t Width = 1600;
        uint32_t Height = 900;
        uint32_t MaxFramesInFlight = 2;
        bool VSync = false;

        bool EnableComputeQueue = true;
        bool EnableTransferQueue = false;

        std::vector<const char*> RequiredVulkanInstanceExtensions;
    };

    class DeviceManager
    {
    public:
        DeviceManager(const DeviceManager&) = delete;
        DeviceManager& operator=(const DeviceManager&) = delete;
        virtual ~DeviceManager() = default;

        // Lifecycle
        [[nodiscard]] static auto Create(const Ref<Window>& window, const DeviceParams& params) -> ScopedPtr<DeviceManager>;
        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        // Frame
        auto BeginFrame() -> bool { return m_Swapchain->BeginFrame(); }
        auto Present() -> bool { return m_Swapchain->Present(); }
        [[nodiscard]] auto WaitIdle() const -> bool;

        // Renderer
        auto InitRenderer() -> void;
        [[nodiscard]] constexpr auto GetRenderer() const -> const ScopedPtr<Renderer>& { return m_Renderer; }

        // Swapchain
        virtual auto CreateSwapchain(GLFWwindow* window, uint32_t width = 0, uint32_t height = 0) -> Ref<Swapchain> = 0;
        [[nodiscard]] auto GetSwapchain() const -> const Ref<Swapchain>& { return m_Swapchain; }
        [[nodiscard]] auto GetCurrentFrameIndex() const -> uint32_t { return m_Swapchain->GetCurrentFrameIndex(); }
        [[nodiscard]] auto GetMaxFramesInFlight() const -> uint32_t { return m_Swapchain->GetMaxFramesInFlight(); }
        [[nodiscard]] auto GetCurrentBackBufferIndex() const -> uint32_t { return m_Swapchain->GetCurrentBackBufferIndex(); }
        [[nodiscard]] auto GetBackBufferCount() const -> uint32_t { return m_Swapchain->GetImageCount(); }
        [[nodiscard]] auto GetCurrentSwapchainImage() const -> const SwapchainImage& { return m_Swapchain->GetCurrentSwapchainImage(); }

        // Nvrhi device
        [[nodiscard]] virtual auto GetDevice() const -> nvrhi::IDevice* = 0;

        // Device Manager
        [[nodiscard]] auto GetParams() -> const DeviceParams& { return m_Params; }
        static auto Get() -> Ref<DeviceManager>;

    protected:
        DeviceManager(const Ref<Window>& window, DeviceParams params);

    protected:
        DeviceParams m_Params;
        ScopedPtr<Renderer> m_Renderer = nullptr;
        Ref<Window> m_Window = nullptr;
        Ref<Swapchain> m_Swapchain = nullptr;

        NvrhiMessageCallback m_MessageCallback;
    };
}
