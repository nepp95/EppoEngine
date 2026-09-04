#pragma once

#include "Renderer/Framebuffer.h"

struct GLFWwindow;

namespace Eppo
{
    struct SwapchainImage
    {
        void* NativeImage = nullptr;
        Ref<Framebuffer> Framebuffer = nullptr;
    };

    class Swapchain
    {
    public:
        virtual ~Swapchain() = default;

        virtual auto BeginFrame() -> bool = 0;
        virtual auto Present() -> bool = 0;
        auto Resize(uint32_t width = 0, uint32_t height = 0) -> void;

        [[nodiscard]] constexpr auto GetCurrentFrameIndex() const -> uint32_t { return m_CurrentFrameIndex; }
        [[nodiscard]] constexpr auto GetMaxFramesInFlight() const -> uint32_t { return m_MaxFramesInFlight; }
        [[nodiscard]] constexpr auto GetCurrentBackBufferIndex() const -> uint32_t { return m_SwapchainImageIndex; }
        [[nodiscard]] auto GetImageCount() const -> uint32_t { return static_cast<uint32_t>(m_Images.size()); }
        [[nodiscard]] auto GetCurrentSwapchainImage() const -> const SwapchainImage& { return m_Images.at(m_SwapchainImageIndex); }
        [[nodiscard]] constexpr auto GetWidth() const -> uint32_t { return m_Width; }
        [[nodiscard]] constexpr auto GetHeight() const -> uint32_t { return m_Height; }

    protected:
        explicit Swapchain(GLFWwindow* window);

        [[nodiscard]] auto GetWindowFramebufferSize() const -> std::pair<uint32_t, uint32_t>;
        virtual auto CreateSwapchain(uint32_t width, uint32_t height) -> void = 0;

    protected:
        GLFWwindow* m_Window = nullptr;
        std::vector<SwapchainImage> m_Images;

        bool m_FrameActive = false;
        bool m_ResizePending = false;
        uint32_t m_CurrentFrameIndex = 0; // Index into frame synchronization data
        uint32_t m_MaxFramesInFlight = 1;
        uint32_t m_SwapchainImageIndex = 0; // Index into m_Images
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };
}
