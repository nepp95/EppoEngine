#include "pch.h"
#include "Platform/Vulkan/PhysicalDevice.h"

#include <GLFW/glfw3.h>

#include <ranges>

namespace Eppo
{
    namespace
    {
        const std::unordered_map<uint32_t, std::string> s_GpuVendors = {
            { 0x1002, "AMD"      },
            { 0x1010, "ImgTec"   },
            { 0x10de, "NVIDIA"   },
            { 0x13b5, "ARM"      },
            { 0x5143, "Qualcomm" },
            { 0x8086, "Intel"    }
        };

        constexpr auto DecodeDriverVersion(const uint32_t driverVersion, const uint32_t vendorId) -> std::string
        {
            std::string result = "Unknown version";

            switch (vendorId)
            {
                // Nvidia
                case 0x10de:
                {
                    const uint32_t d1 = (driverVersion >> 22) & 0x3ff;
                    const uint32_t d2 = (driverVersion >> 14) & 0x0ff;
                    const uint32_t d3 = (driverVersion >> 6) & 0x0ff;
                    const uint32_t d4 = driverVersion & 0x003f;

                    result = std::to_string(d1) + "." + std::to_string(d2) + "." + std::to_string(d3) + "." + std::to_string(d4);
                    break;
                }

                // Intel
                case 0x8086:
                {
                    const uint32_t d1 = driverVersion >> 14;
                    const uint32_t d2 = driverVersion & 0x3ff;

                    result = std::to_string(d1) + "." + std::to_string(d2);
                    break;
                }

                default:
                {
                    const uint32_t d1 = driverVersion >> 22;
                    const uint32_t d2 = (driverVersion >> 12) & 0x3ff;
                    const uint32_t d3 = driverVersion & 0xfff;

                    result = std::to_string(d1) + "." + std::to_string(d2) + "." + std::to_string(d3);
                }
            }

            return result;
        }
    }

    PhysicalDevice::PhysicalDevice(VkInstance instance)
    {
        // Get physical devices available
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "Failed to enumerate physical devices!");

        if (deviceCount <= 0)
        {
            Log::Error("No physical GPU's found!");
            return;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "Failed to enumerate physical devices!");

        // Select appropriate physical device
        for (const auto& device : devices)
        {
            vkGetPhysicalDeviceProperties(device, &m_Properties);
            if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                m_Device = device;
                break;
            }
        }

        // No discrete GPU available, select first GPU possible
        if (m_Device == nullptr)
        {
            Log::Warn("No discrete GPU found, falling back to integrated GPU!");
            m_Device = devices.back();
        }

        // Get device information
        vkGetPhysicalDeviceMemoryProperties(m_Device, &m_MemoryProperties);

        m_Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        vkGetPhysicalDeviceFeatures2(m_Device, &m_Features);

        // Output device information
        Log::Info("GPU Info:");
        Log::Info("\tVendor: {}", s_GpuVendors.contains(m_Properties.vendorID) ? s_GpuVendors.at(m_Properties.vendorID) : "Unknown");
        Log::Info("\tModel: {}", m_Properties.deviceName);
        Log::Info("\tDriver version: {}", DecodeDriverVersion(m_Properties.driverVersion, m_Properties.vendorID));

        // Get device queue families
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_Device, &count, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_Device, &count, queueFamilies.data());

        for (size_t i = 0; i < queueFamilies.size(); i++)
        {
            const auto& queueFam = queueFamilies.at(i);

            if (queueFam.queueCount == 0)
                continue;

            if (m_QueueFamilyIndices.Graphics == -1)
            {
                if (queueFam.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    m_QueueFamilyIndices.Graphics = static_cast<int32_t>(i);
                }
            }

            if (m_QueueFamilyIndices.Compute == -1)
            {
                if (queueFam.queueFlags & VK_QUEUE_COMPUTE_BIT && !(queueFam.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    m_QueueFamilyIndices.Compute = static_cast<int32_t>(i);
                }
            }

            if (m_QueueFamilyIndices.Transfer == -1)
            {
                if (queueFam.queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFam.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    !(queueFam.queueFlags & VK_QUEUE_COMPUTE_BIT))
                {
                    m_QueueFamilyIndices.Transfer = static_cast<int32_t>(i);
                }
            }

            if (m_QueueFamilyIndices.Present == -1)
            {
                if (glfwGetPhysicalDevicePresentationSupport(instance, m_Device, static_cast<uint32_t>(i)))
                {
                    m_QueueFamilyIndices.Present = static_cast<int32_t>(i);
                }
            }

            if (m_QueueFamilyIndices.IsComplete())
                break;
        }

        if (!m_QueueFamilyIndices.IsComplete())
        {
            Log::Error("Failed to find queue family indices for either present, graphics or both!");
            return;
        }

        // Find supported extensions
        uint32_t extensionCount = 0;
        VK_CHECK(
            vkEnumerateDeviceExtensionProperties(m_Device, nullptr, &extensionCount, nullptr),
            "Failed to enumerate device extension properties!"
        );

        std::vector<VkExtensionProperties> extensions(extensionCount);
        VK_CHECK(
            vkEnumerateDeviceExtensionProperties(m_Device, nullptr, &extensionCount, extensions.data()),
            "Failed to enumerate device extension properties!"
        );

        m_SupportedExtensions.resize(extensionCount);
        for (uint32_t i = 0; i < extensionCount; i++)
            m_SupportedExtensions[i] = extensions.at(i).extensionName;
    }

    auto PhysicalDevice::IsExtensionSupported(std::string_view extensionName) -> bool
    {
        return std::ranges::find(m_SupportedExtensions, extensionName) != m_SupportedExtensions.end();
    }
}