#include "pch.h"
#include "Platform/Vulkan/LogicalDevice.h"

#include "Renderer/DeviceManager.h"

namespace Eppo
{
    LogicalDevice::LogicalDevice(const ScopedPtr<PhysicalDevice>& physicalDevice)
    {
        // Check extension support
        for (const auto& extension : g_DeviceExtensions)
        {
            if (!physicalDevice->IsExtensionSupported(extension))
            {
                Log::Error("Device extension '{}' is not supported by the device!", extension);
                return;
            }

            Log::Info("Enabled device extension '{}'", extension);
        }

        if (!physicalDevice->SupportsRequiredFeatures())
        {
            Log::Error("Device does not support all required Vulkan features!");
            return;
        }

        // Create device queue infos
        const auto& indices = physicalDevice->GetQueueFamilyIndices();
        constexpr float queuePriority = 1.0f;

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (const auto& index : indices.GetUniqueIndices())
        {
            auto& queueInfo = queueInfos.emplace_back();
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueCount = 1;
            queueInfo.queueFamilyIndex = index;
            queueInfo.pQueuePriorities = &queuePriority;
        }

        // Create device
        VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT featuresMutable{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT,
            .mutableDescriptorType = VK_TRUE,
        };

        VkPhysicalDeviceVulkan11Features features11{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = &featuresMutable,
        };

        VkPhysicalDeviceVulkan12Features features12{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features11,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
            .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
            .timelineSemaphore = VK_TRUE,
            .bufferDeviceAddress = VK_TRUE,
            .shaderOutputLayer = VK_TRUE,
        };

        VkPhysicalDeviceVulkan13Features features13{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &features12,
            .shaderDemoteToHelperInvocation = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE,
        };

        VkPhysicalDeviceFeatures2 deviceFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &features13,
            .features = physicalDevice->GetDeviceFeatures().features,
        };

        VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &deviceFeatures,
            .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(g_DeviceExtensions.size()),
            .ppEnabledExtensionNames = g_DeviceExtensions.data(),
        };

        if (s_EnableValidationLayers)
        {
            deviceInfo.enabledLayerCount = static_cast<uint32_t>(g_ValidationLayers.size());
            deviceInfo.ppEnabledLayerNames = g_ValidationLayers.data();
        }
        else
        {
            deviceInfo.enabledLayerCount = 0;
            deviceInfo.ppEnabledLayerNames = nullptr;
        }

        VK_CHECK(vkCreateDevice(physicalDevice->GetNative(), &deviceInfo, nullptr, &m_Device), "Failed to create device!");
        EP_ASSERT(m_Device);

        // Get device queues
        vkGetDeviceQueue(m_Device, indices.Graphics, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, indices.Compute, 0, &m_ComputeQueue);
        vkGetDeviceQueue(m_Device, indices.Transfer, 0, &m_TransferQueue);
        vkGetDeviceQueue(m_Device, indices.Present, 0, &m_PresentQueue);
    }

    LogicalDevice::~LogicalDevice()
    {
        vkDestroyDevice(m_Device, nullptr);
    }
}
