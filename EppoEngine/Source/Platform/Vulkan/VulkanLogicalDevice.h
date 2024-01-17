#pragma once

#include "Platform/Vulkan/VulkanPhysicalDevice.h"

namespace Eppo
{
	class VulkanLogicalDevice
	{
	public:
		VulkanLogicalDevice(const Ref<VulkanPhysicalDevice>& physicalDevice);

		VkCommandBuffer GetCommandBuffer(bool begin);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);
		VkCommandBuffer GetSecondaryCommandBuffer();

		const Ref<VulkanPhysicalDevice>& GetPhysicalDevice() const { return m_PhysicalDevice; }
		const VkDevice& GetNativeDevice() const { return m_Device; }
		const VkQueue& GetGraphicsQueue() const { return m_GraphicsQueue; }

	private:
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;

		VkDevice m_Device;
		VkQueue m_GraphicsQueue;
		VkCommandPool m_CommandPool;
	};
}
