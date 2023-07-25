#pragma once

#include "Renderer/Vulkan.h"

namespace Eppo
{
	struct QueueFamilyIndices
	{
		int32_t Graphics = -1;

		bool IsComplete()
		{
			return Graphics > -1;
		}
	};

	class PhysicalDevice
	{
	public:
		PhysicalDevice();

		const VkPhysicalDevice& GetNativeDevice() const { return m_Device; }

		const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_Features; }
		const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const { return m_MemoryProperties; }
		const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_Properties; }

		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_Indices; }

		bool IsExtensionSupported(const std::string& extension);
	
	private:
		QueueFamilyIndices FindQueueFamilyIndices();

	private:
		VkPhysicalDevice m_Device;
		VkPhysicalDeviceFeatures m_Features;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;
		VkPhysicalDeviceProperties m_Properties;

		QueueFamilyIndices m_Indices;
		std::vector<std::string> m_SupportedExtensions;
	};
}
