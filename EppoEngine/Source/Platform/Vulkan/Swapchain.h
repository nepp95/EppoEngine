#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/DeviceManager.h"

namespace Eppo
{
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class Swapchain
    {
    public:
        Swapchain(VkSurfaceKHR surface);
        ~Swapchain();

        auto BeginFrame() -> bool;
        auto Present() -> bool;

        auto CreateSwapchain(uint32_t width = 0, uint32_t height = 0) -> void;
        auto Resize(uint32_t width = 0, uint32_t height = 0) -> void;

        auto GetCurrentFrameIndex() const -> uint32_t { return m_CurrentFrameIndex; }
        auto GetMaxFramesInFlight() const -> uint32_t { return m_MaxFramesInFlight; }
        auto GetCurrentBackBufferIndex() const -> uint32_t { return m_SwapchainImageIndex; }
        auto GetImageCount() const -> uint32_t { return static_cast<uint32_t>(m_Images.size()); }
        auto GetCurrentSwapchainImage() -> const SwapchainImage& { return m_Images.at(m_SwapchainImageIndex); }

    private:
        [[nodiscard]] auto QuerySwapchainSupportDetails() const -> SwapchainSupportDetails;
        [[nodiscard]] auto SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) -> VkSurfaceFormatKHR;
        [[nodiscard]] auto SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool vsync) -> VkPresentModeKHR;
        [[nodiscard]] auto SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities) const -> VkExtent2D;

    private:
        VkSwapchainKHR m_Swapchain = nullptr;
        VkSurfaceKHR m_Surface = nullptr;

        std::vector<SwapchainImage> m_Images;

        VkFormat m_Format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};
        VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSurfaceFormatKHR m_SurfaceFormat;

        struct FrameSync
        {
            VkSemaphore AcquireSemaphore = nullptr;
            nvrhi::EventQueryHandle CompletionQuery = nullptr;
            bool InFlight = false;
        };

        std::vector<FrameSync> m_FrameSyncData;
        std::vector<VkSemaphore> m_PresentSemaphores;
        bool m_FrameActive = false;
        bool m_ResizePending = false;
        uint32_t m_CurrentFrameIndex = 0; // Index into m_FrameSyncData
        uint32_t m_MaxFramesInFlight = 1;
        uint32_t m_SwapchainImageIndex = 0; // Index into m_Images
    };
}
