#pragma once

#include "Platform/Vulkan/Vulkan.h"

namespace Eppo
{
	struct QueueFamilyIndices
	{
		int32_t Graphics = -1;
		int32_t Present = -1;

		bool IsComplete() const
		{
			return Graphics > -1 && Present > -1;
		}
	};

	class VulkanPhysicalDevice
	{
	public:
		VulkanPhysicalDevice();
		~VulkanPhysicalDevice() = default;

		VkPhysicalDevice GetNativeDevice() const { return m_PhysicalDevice; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }

		QueueFamilyIndices& GetQueueFamilyIndices() { return m_QueueFamilyIndices; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

		const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_Properties; }
		const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const { return m_MemoryProperties; }
		const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_Features; }

		bool IsExtensionSupported(std::string_view extension);

	private:
		QueueFamilyIndices FindQueueFamilyIndices() const;

	private:
		VkPhysicalDevice m_PhysicalDevice;
		VkPhysicalDeviceProperties m_Properties;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;
		VkPhysicalDeviceFeatures m_Features;

		VkSurfaceKHR m_Surface;

		QueueFamilyIndices m_QueueFamilyIndices;
		std::vector<std::string> m_SupportedExtensions;
	};
}
