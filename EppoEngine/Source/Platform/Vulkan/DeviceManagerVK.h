#pragma once

#include "Platform/Vulkan/LogicalDevice.h"
#include "Platform/Vulkan/PhysicalDevice.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/DeviceManager.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/validation.h>
#include <nvrhi/vulkan.h>

namespace Eppo
{
    class DeviceManagerVK : public DeviceManager
    {
    public:
        explicit DeviceManagerVK(const Ref<Window>& window, const DeviceParams& params);
        ~DeviceManagerVK() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto CreateSwapchain(GLFWwindow* window, uint32_t width, uint32_t height) -> Ref<Swapchain> override;

        [[nodiscard]] auto GetDevice() const -> nvrhi::IDevice* override;

        [[nodiscard]] constexpr auto GetVulkanInstance() const -> VkInstance { return m_Instance; }
        [[nodiscard]] constexpr auto GetPhysicalDevice() const -> const ScopedPtr<PhysicalDevice>& { return m_PhysicalDevice; }
        [[nodiscard]] constexpr auto GetLogicalDevice() const -> const ScopedPtr<LogicalDevice>& { return m_LogicalDevice; }

    private:
        auto CreateVulkanInstance() -> void;
        auto CreateNvrhiDevice() -> void;

    private:
        nvrhi::vulkan::DeviceHandle m_Device = nullptr;
        nvrhi::DeviceHandle m_ValidationLayer = nullptr;

        VkInstance m_Instance = nullptr;
        VkDebugUtilsMessengerEXT m_DebugMessenger = nullptr;

        ScopedPtr<PhysicalDevice> m_PhysicalDevice = nullptr;
        ScopedPtr<LogicalDevice> m_LogicalDevice = nullptr;
    };
}
