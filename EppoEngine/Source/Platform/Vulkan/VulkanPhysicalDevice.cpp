#include "pch.h"
#include "VulkanPhysicalDevice.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
		EPPO_PROFILE_FUNCTION("PhysicalDevice::PhysicalDevice");

		Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
		VkInstance instance = context->GetVulkanInstance();

		// Select GPU
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		// If there are no physical devices, we can't continue
		EPPO_ASSERT((deviceCount > 0));

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			vkGetPhysicalDeviceProperties(device, &m_Properties);
			if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				m_Device = device;
				break;
			}
		}

		if (!m_Device)
		{
			EPPO_WARN("No discrete GPU found, using integrated GPU!");
			m_Device = devices.back();
		}

		EPPO_ASSERT(m_Device);

		// Get properties/features from device
		vkGetPhysicalDeviceFeatures(m_Device, &m_Features);
		vkGetPhysicalDeviceMemoryProperties(m_Device, &m_MemoryProperties);

		// Queue family indices
		m_Indices = FindQueueFamilyIndices();

		// Device extensions
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(m_Device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(m_Device, nullptr, &extensionCount, availableExtensions.data());

		EPPO_INFO("Selected device has {} extensions", extensionCount);

		for (const auto& extension : availableExtensions)
			m_SupportedExtensions.emplace_back(extension.extensionName);
	}

	QueueFamilyIndices VulkanPhysicalDevice::FindQueueFamilyIndices()
	{
		EPPO_PROFILE_FUNCTION("PhysicalDevice::FindQueueFamilyIndices");

		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_Device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_Device, &queueFamilyCount, queueFamilies.data());

		for (size_t i = 0; i < queueFamilies.size(); i++)
		{
			if (queueFamilies[i].queueFlags &&
				VK_QUEUE_GRAPHICS_BIT &&
				queueFamilies[i].timestampValidBits > 0
				)
			{
				indices.Graphics = (int32_t)i;
			}

			if (indices.IsComplete())
				break;
		}

		return indices;
	}

	bool VulkanPhysicalDevice::IsExtensionSupported(const std::string& extension)
	{
		EPPO_PROFILE_FUNCTION("PhysicalDevice::IsExtensionSupported");

		return std::find(m_SupportedExtensions.begin(), m_SupportedExtensions.end(), extension) != m_SupportedExtensions.end();
	}
}
