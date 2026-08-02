#include "pch.h"
#include "Platform/Vulkan/PhysicalDevice.h"

#include <GLFW/glfw3.h>

#include <ranges>

namespace Eppo
{
    namespace
    {
        constexpr uint32_t s_GpuVendorAMD = 0x1002;
        constexpr uint32_t s_GpuVendorImgTec = 0x1010;
        constexpr uint32_t s_GpuVendorNVIDIA = 0x10de;
        constexpr uint32_t s_GpuVendorARM = 0x13b5;
        constexpr uint32_t s_GpuVendorQualcomm = 0x5143;
        constexpr uint32_t s_GpuVendorIntel = 0x8086;

        const std::unordered_map<uint32_t, std::string> s_GpuVendors = {
            { s_GpuVendorAMD,      "AMD"      },
            { s_GpuVendorImgTec,   "ImgTec"   },
            { s_GpuVendorNVIDIA,   "NVIDIA"   },
            { s_GpuVendorARM,      "ARM"      },
            { s_GpuVendorQualcomm, "Qualcomm" },
            { s_GpuVendorIntel,    "Intel"    }
        };

        constexpr auto DecodeDriverVersion(const uint32_t driverVersion, const uint32_t vendorId) -> std::string
        {
            std::string result = "Unknown version";

            switch (vendorId)
            {
                // Nvidia
                case s_GpuVendorNVIDIA:
                {
                    const uint32_t d1 = (driverVersion >> 22) & 0x3ff;
                    const uint32_t d2 = (driverVersion >> 14) & 0x0ff;
                    const uint32_t d3 = (driverVersion >> 6) & 0x0ff;
                    const uint32_t d4 = driverVersion & 0x003f;

                    result = std::to_string(d1) + "." + std::to_string(d2) + "." + std::to_string(d3) + "." + std::to_string(d4);
                    break;
                }

                // Intel
                case s_GpuVendorIntel:
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

        m_FeaturesMutable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT;
        m_Features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        m_Features11.pNext = &m_FeaturesMutable;
        m_Features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        m_Features12.pNext = &m_Features11;
        m_Features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        m_Features13.pNext = &m_Features12;
        m_Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        m_Features.pNext = &m_Features13;
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

    auto PhysicalDevice::IsExtensionSupported(const std::string_view extensionName) -> bool
    {
        return std::ranges::find(m_SupportedExtensions, extensionName) != m_SupportedExtensions.end();
    }

    auto PhysicalDevice::SupportsRequiredFeatures() const -> bool
    {
        auto supported = true;
        const auto RequireFeature = [&supported](const VkBool32 feature, const std::string_view name) -> void
        {
            if (feature)
                return;

            Log::Error("Required Vulkan feature '{}' is not supported by the device!", name);
            supported = false;
        };

        RequireFeature(m_FeaturesMutable.mutableDescriptorType, "mutableDescriptorType");
        RequireFeature(m_Features12.shaderSampledImageArrayNonUniformIndexing, "shaderSampledImageArrayNonUniformIndexing");
        RequireFeature(m_Features12.descriptorBindingUniformBufferUpdateAfterBind, "descriptorBindingUniformBufferUpdateAfterBind");
        RequireFeature(m_Features12.descriptorBindingSampledImageUpdateAfterBind, "descriptorBindingSampledImageUpdateAfterBind");
        RequireFeature(m_Features12.descriptorBindingStorageImageUpdateAfterBind, "descriptorBindingStorageImageUpdateAfterBind");
        RequireFeature(m_Features12.descriptorBindingStorageBufferUpdateAfterBind, "descriptorBindingStorageBufferUpdateAfterBind");
        RequireFeature(
            m_Features12.descriptorBindingUniformTexelBufferUpdateAfterBind, "descriptorBindingUniformTexelBufferUpdateAfterBind"
        );
        RequireFeature(
            m_Features12.descriptorBindingStorageTexelBufferUpdateAfterBind, "descriptorBindingStorageTexelBufferUpdateAfterBind"
        );
        RequireFeature(m_Features12.descriptorBindingUpdateUnusedWhilePending, "descriptorBindingUpdateUnusedWhilePending");
        RequireFeature(m_Features12.descriptorBindingPartiallyBound, "descriptorBindingPartiallyBound");
        RequireFeature(m_Features12.runtimeDescriptorArray, "runtimeDescriptorArray");
        RequireFeature(m_Features12.timelineSemaphore, "timelineSemaphore");
        RequireFeature(m_Features12.bufferDeviceAddress, "bufferDeviceAddress");
        RequireFeature(m_Features12.shaderOutputLayer, "shaderOutputLayer");
        RequireFeature(m_Features13.shaderDemoteToHelperInvocation, "shaderDemoteToHelperInvocation");
        RequireFeature(m_Features13.synchronization2, "synchronization2");
        RequireFeature(m_Features13.dynamicRendering, "dynamicRendering");

        return supported;
    }
}
