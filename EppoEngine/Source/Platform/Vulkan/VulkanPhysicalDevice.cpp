#include "pch.h"
#include "VulkanPhysicalDevice.h"

#include "Platform/Vulkan/VulkanContext.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	static std::string DecodeDriverVersion(uint32_t driverVersion, uint32_t vendorId)
	{
		std::string versionStr = "Unknown version";

		switch (vendorId)
		{
			// Nvidia
			case 0x10DE:
			{
				uint32_t d1 = (driverVersion >> 22) & 0x3ff;
				uint32_t d2 = (driverVersion >> 14) & 0x0ff;
				uint32_t d3 = (driverVersion >> 6) & 0x0ff;
				uint32_t d4 = driverVersion & 0x003f;

				versionStr = std::to_string(d1) + "." + std::to_string(d2) + "." + std::to_string(d3) + "." + std::to_string(d4);
				break;
			}

			// Intel
			case 0x8086:
			{
				uint32_t d1 = driverVersion >> 14;
				uint32_t d2 = driverVersion & 0x3ff;

				versionStr = std::to_string(d1) + "." + std::to_string(d2);
				break;
			}

			default:
			{
				uint32_t d1 = driverVersion >> 22;
				uint32_t d2 = (driverVersion >> 12) & 0x3ff;
				uint32_t d3 = driverVersion & 0xfff;

				versionStr = std::to_string(d1) + "." + std::to_string(d2) + "." + std::to_string(d3);
			}
		}

		return versionStr;
	}

	static const std::unordered_map<uint32_t, std::string> s_GpuVendors = {
		{ 0x1002, "AMD" },
		{ 0x1010, "ImgTec" },
		{ 0x10DE, "NVIDIA" },
		{ 0x13B5, "ARM" },
		{ 0x5143, "Qualcomm" },
		{ 0x8086, "Intel" }
	};

	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
		auto instance = VulkanContext::GetVulkanInstance();

		// Get physical devices available
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		EPPO_ASSERT(deviceCount > 0);

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		// Select physical device, we prefer a discrete GPU
		for (const auto& device : devices)
		{
			vkGetPhysicalDeviceProperties(device, &m_Properties);
			if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				m_PhysicalDevice = device;
				break;
			}
		}

		// No discrete GPU available, select first GPU possible
		if (!m_PhysicalDevice)
		{
			EPPO_WARN("No discrete GPU found!");
			m_PhysicalDevice = devices.back();
			EPPO_ASSERT(m_PhysicalDevice);
		}

		// Get properties and features from selected device
		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_Features);
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);

		EPPO_INFO("GPU Info:\n"
			"\t\t\tVendor: {}\n"
			"\t\t\tModel: {}\n"
			"\t\t\tDriver version: {}",
			s_GpuVendors.find(m_Properties.vendorID) != s_GpuVendors.end() ? s_GpuVendors.at(m_Properties.vendorID) : "Unknown",
			m_Properties.deviceName,
			DecodeDriverVersion(m_Properties.driverVersion, m_Properties.vendorID));

		// Create surface
		Ref<VulkanContext> context = VulkanContext::Get();
		VK_CHECK(glfwCreateWindowSurface(VulkanContext::GetVulkanInstance(), context->GetWindowHandle(), nullptr, &m_Surface), "Failed to create surface!");

		context->SubmitResourceFree([this]()
		{
			EPPO_WARN("Releasing surface {}", (void*)this);
			vkDestroySurfaceKHR(VulkanContext::GetVulkanInstance(), m_Surface, nullptr);
		});

		// Queue family indices
		m_QueueFamilyIndices = FindQueueFamilyIndices();

		// Device extensions
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

		EPPO_INFO("Selected device has {} extensions", availableExtensions.size());
		for (const auto& extension : availableExtensions)
			m_SupportedExtensions.emplace_back(extension.extensionName);
	}

	bool VulkanPhysicalDevice::IsExtensionSupported(std::string_view extension)
	{
		return std::find(m_SupportedExtensions.begin(), m_SupportedExtensions.end(), extension) != m_SupportedExtensions.end();
	}

	QueueFamilyIndices VulkanPhysicalDevice::FindQueueFamilyIndices() const
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

		for (size_t i = 0; i < queueFamilies.size(); i++)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.Graphics = static_cast<int32_t>(i);

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);

			if (presentSupport)
				indices.Present = static_cast<int32_t>(i);

			if (indices.IsComplete())
				break;
		}

		return indices;
	}
}
