#include "pch.h"
#include "Platform/Vulkan/VulkanSwapchain.h"

#include "Platform/Vulkan/DeviceManagerVK.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>
#include <nvrhi/vulkan.h>

namespace Eppo
{
    namespace
    {
        auto VulkanFormatToNvrhi(const VkFormat format) -> nvrhi::Format
        {
            switch (format)
            {
                case VK_FORMAT_R8G8B8A8_UNORM:
                    return nvrhi::Format::RGBA8_UNORM;
                case VK_FORMAT_B8G8R8A8_UNORM:
                    return nvrhi::Format::BGRA8_UNORM;
                case VK_FORMAT_R8G8B8A8_SRGB:
                    return nvrhi::Format::SRGBA8_UNORM;
                case VK_FORMAT_B8G8R8A8_SRGB:
                    return nvrhi::Format::SBGRA8_UNORM;
                default:
                    return nvrhi::Format::UNKNOWN;
            }
        }
    }

    VulkanSwapchain::VulkanSwapchain(GLFWwindow* window)
        : Swapchain(window)
    {
        const auto& dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());

        VK_CHECK(glfwCreateWindowSurface(dm->GetVulkanInstance(), window, nullptr, &m_Surface), "Failed to create window surface!");
        EP_ASSERT(m_Surface);

        // Get swapchain support details
        auto [capabilities, formats, presentModes] = QuerySwapchainSupportDetails();
        m_SurfaceFormat = SelectSurfaceFormat(formats);
        m_PresentMode = SelectPresentMode(presentModes, dm->GetParams().VSync);
        m_Format = m_SurfaceFormat.format;
        m_Extent = SelectExtent(capabilities);
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        // Viewport swapchains are destroyed while the device is still alive
        DeviceManager::Get()->WaitIdle();

        const auto& dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());
        VkDevice device = dm->GetLogicalDevice()->GetNative();

        m_Images.clear();

        for (size_t i = 0; i < m_PresentSemaphores.size(); i++)
            vkDestroySemaphore(device, m_PresentSemaphores.at(i), nullptr);

        for (auto& frame : m_FrameSyncData)
            vkDestroySemaphore(device, frame.AcquireSemaphore, nullptr);
        m_FrameSyncData.clear();

        vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
        vkDestroySurfaceKHR(dm->GetVulkanInstance(), m_Surface, nullptr);
    }

    auto VulkanSwapchain::BeginFrame() -> bool
    {
        EP_PROFILE_FN("Swapchain::BeginFrame");
        EP_ASSERT(!m_FrameActive, "BeginFrame was called while a swapchain frame is already active!");

        const auto [width, height] = GetWindowFramebufferSize();
        if (width == 0 || height == 0)
            return false;

        if (width != m_Width || height != m_Height)
            m_ResizePending = true;

        const auto& dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());
        VkDevice device = dm->GetLogicalDevice()->GetNative();
        nvrhi::vulkan::IDevice* vkNvrhiDevice = dm->GetDevice()->getNativeObject(nvrhi::ObjectTypes::Nvrhi_VK_Device);

        constexpr uint32_t maxAttempts = 3;
        VkResult result;

        for (uint32_t attempt = 0; attempt < maxAttempts; attempt++)
        {
            if (m_ResizePending)
                Resize();

            auto& frame = m_FrameSyncData.at(m_CurrentFrameIndex);
            if (frame.InFlight)
            {
                vkNvrhiDevice->waitEventQuery(frame.CompletionQuery);
                vkNvrhiDevice->resetEventQuery(frame.CompletionQuery);
                frame.InFlight = false;
            }

            const VkSemaphore acquireSemaphore = frame.AcquireSemaphore;
            result = vkAcquireNextImageKHR(device, m_Swapchain, UINT64_MAX, acquireSemaphore, nullptr, &m_SwapchainImageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                m_ResizePending = true;
                continue;
            }

            if (result == VK_SUBOPTIMAL_KHR)
                m_ResizePending = true;
            else if (result != VK_SUCCESS)
            {
                Log::Error(LogSource::Vulkan, "Failed to acquire a swapchain image: VkResult {}", static_cast<int32_t>(result));
                return false;
            }

            vkNvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, acquireSemaphore, 0);
            m_FrameActive = true;
            return true;
        }

        Log::Error(LogSource::Vulkan, "Failed to acquire a swapchain image!");
        return false;
    }

    auto VulkanSwapchain::Present() -> bool
    {
        EP_PROFILE_FN("Swapchain::Present");
        EP_ASSERT(m_FrameActive, "Present was called without an active swapchain frame!");

        const auto& dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());
        nvrhi::vulkan::IDevice* vkNvrhiDevice(dm->GetDevice()->getNativeObject(nvrhi::ObjectTypes::Nvrhi_VK_Device));

        auto& frame = m_FrameSyncData.at(m_CurrentFrameIndex);
        const auto& semaphore = m_PresentSemaphores.at(m_SwapchainImageIndex);

        vkNvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);
        vkNvrhiDevice->executeCommandLists(nullptr, 0);
        vkNvrhiDevice->setEventQuery(frame.CompletionQuery, nvrhi::CommandQueue::Graphics);
        frame.InFlight = true;

        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &semaphore,
            .swapchainCount = 1,
            .pSwapchains = &m_Swapchain,
            .pImageIndices = &m_SwapchainImageIndex,
        };

        VkResult result = vkQueuePresentKHR(dm->GetLogicalDevice()->GetPresentQueue(), &presentInfo);
        m_FrameActive = false;
        m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % m_MaxFramesInFlight;

        if (result == VK_SUCCESS)
            return true;

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            m_ResizePending = true;
            return true;
        }

        Log::Error(LogSource::Vulkan, "Failed to present a swapchain image: VkResult {}", static_cast<int32_t>(result));
        return false;
    }

    auto VulkanSwapchain::CreateSwapchain(uint32_t width, uint32_t height) -> void
    {
        const auto& dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());
        VkDevice device = dm->GetLogicalDevice()->GetNative();

        if (m_Swapchain)
        {
            for (const VkSemaphore semaphore : m_PresentSemaphores)
                vkDestroySemaphore(device, semaphore, nullptr);
            m_PresentSemaphores.clear();
        }

        m_Images.clear();

        auto [capabilities, formats, presentModes] = QuerySwapchainSupportDetails();
        const auto& indices = dm->GetPhysicalDevice()->GetQueueFamilyIndices();
        VkSwapchainKHR oldSwapchain = m_Swapchain ? m_Swapchain : nullptr;

        if (width == 0 || height == 0)
            m_Extent = SelectExtent(capabilities);
        else
            m_Extent = { width, height };

        m_Width = m_Extent.width;
        m_Height = m_Extent.height;

        VkSwapchainCreateInfoKHR swapchainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = m_Surface,
            .minImageCount = capabilities.minImageCount,
            .imageFormat = m_Format,
            .imageColorSpace = m_SurfaceFormat.colorSpace,
            .imageExtent = m_Extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = indices.Graphics == indices.Present ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = m_PresentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = oldSwapchain,
        };

        VK_CHECK(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &m_Swapchain), "Failed to create swapchain!");
        EP_ASSERT(m_Swapchain);

        if (oldSwapchain)
            vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

        uint32_t swapchainImageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(device, m_Swapchain, &swapchainImageCount, nullptr), "Failed to get swapchain images!");
        EP_ASSERT(swapchainImageCount >= 2);
        m_PresentSemaphores.resize(swapchainImageCount);
        m_MaxFramesInFlight = std::min(DeviceManager::Get()->GetParams().MaxFramesInFlight, swapchainImageCount);

        std::vector<VkImage> images(swapchainImageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(device, m_Swapchain, &swapchainImageCount, images.data()), "Failed to get swapchain images!");

        constexpr VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (m_FrameSyncData.size() != m_MaxFramesInFlight)
        {
            for (const auto& frame : m_FrameSyncData)
                vkDestroySemaphore(device, frame.AcquireSemaphore, nullptr);

            m_FrameSyncData.clear();
            m_FrameSyncData.resize(m_MaxFramesInFlight);

            for (auto& frame : m_FrameSyncData)
            {
                VkSemaphore acquireSemaphore = nullptr;
                VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquireSemaphore), "Failed to create acquire semaphore!");
                frame.AcquireSemaphore = acquireSemaphore;
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

        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            // Create image views
            auto& image = m_Images.emplace_back();
            image.NativeImage = images.at(i);

            const nvrhi::Format imageFormat = VulkanFormatToNvrhi(m_Format);
            EP_ASSERT(imageFormat != nvrhi::Format::UNKNOWN);
            ImageSpecification imageSpec{
                .ImageFormat = imageFormat,
                .Width = m_Extent.width,
                .Height = m_Extent.height,
                .IsRenderTarget = true,
                .InitialState = nvrhi::ResourceStates::Present,
                .DebugName = std::format("Swapchain Image {}", i),
            };

            FramebufferSpecification framebufferSpec{
                .Width = m_Extent.width,
                .Height = m_Extent.height,
                .SwapchainTarget = true,
                .SwapchainImage = Image::Create(imageSpec, image.NativeImage),
                .DebugName = std::format("Swapchain Framebuffer {}", i),
            };

            image.Framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            // Create present semaphores
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_PresentSemaphores[i]), "Failed to create semaphore!");
        }
    }

    auto VulkanSwapchain::QuerySwapchainSupportDetails() const -> SwapchainSupportDetails
    {
        const auto& dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());
        const auto& physicalDevice = dm->GetPhysicalDevice();

        SwapchainSupportDetails details;

        // Capabilities
        VK_CHECK(
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice->GetNative(), m_Surface, &details.Capabilities),
            "Failed to get swapchain surface capabilities!"
        );

        // Formats
        uint32_t formatCount = 0;
        VK_CHECK(
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice->GetNative(), m_Surface, &formatCount, nullptr),
            "Failed to get swapchain surface format!"
        );

        if (formatCount > 0)
        {
            details.Formats.resize(formatCount);
            VK_CHECK(
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice->GetNative(), m_Surface, &formatCount, details.Formats.data()),
                "Failed to get swapchain surface format!"
            );
        }

        // Present modes
        uint32_t presentModeCount = 0;
        VK_CHECK(
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice->GetNative(), m_Surface, &presentModeCount, nullptr),
            "Failed to get swapchain present modes!"
        );

        if (presentModeCount > 0)
        {
            details.PresentModes.resize(presentModeCount);
            VK_CHECK(
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physicalDevice->GetNative(), m_Surface, &presentModeCount, details.PresentModes.data()
                ),
                "Failed to get swapchain present modes!"
            );
        }

        return details;
    }

    auto VulkanSwapchain::SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) -> VkSurfaceFormatKHR
    {
        EP_ASSERT(!surfaceFormats.empty(), "The Vulkan device reported no supported surface formats.");

        constexpr std::array preferredFormats{
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
        };

        for (const VkFormat preferred : preferredFormats)
        {
            for (const auto& format : surfaceFormats)
            {
                if (format.format == preferred && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    return format;
            }
        }

        Log::Warn("Can't find a supported RGBA8 swapchain format, falling back to first found!");
        return surfaceFormats.front();
    }

    auto VulkanSwapchain::SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes, const bool vsync) -> VkPresentModeKHR
    {
        if (vsync)
            return VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& mode : presentModes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode;
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    auto VulkanSwapchain::SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities) const -> VkExtent2D
    {
        VkExtent2D extent = capabilities.currentExtent;

        if (capabilities.currentExtent.width == UINT32_MAX)
        {
            const auto [width, height] = GetWindowFramebufferSize();

            extent = { .width = width, .height = height };
            extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        return extent;
    }
}
