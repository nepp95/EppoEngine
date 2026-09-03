#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Swapchain.h"

namespace Eppo
{
    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class VulkanSwapchain : public Swapchain
    {
    public:
        explicit VulkanSwapchain(GLFWwindow* window);
        ~VulkanSwapchain() override;

        auto BeginFrame() -> bool override;
        auto Present() -> bool override;

        auto CreateSwapchain(uint32_t width = 0, uint32_t height = 0) -> void override;

    private:
        [[nodiscard]] auto QuerySwapchainSupportDetails() const -> SwapchainSupportDetails;
        [[nodiscard]] auto SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) -> VkSurfaceFormatKHR;
        [[nodiscard]] auto SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool vsync) -> VkPresentModeKHR;
        [[nodiscard]] auto SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities) const -> VkExtent2D;

    private:
        struct FrameSync
        {
            VkSemaphore AcquireSemaphore = nullptr;
            nvrhi::EventQueryHandle CompletionQuery = nullptr;
            bool InFlight = false;
        };

        std::vector<FrameSync> m_FrameSyncData;
        VkSwapchainKHR m_Swapchain = nullptr;
        VkSurfaceKHR m_Surface = nullptr;

        VkFormat m_Format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};
        VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkSurfaceFormatKHR m_SurfaceFormat{};

        std::vector<VkSemaphore> m_PresentSemaphores;
    };
}
