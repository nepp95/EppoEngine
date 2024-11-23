#pragma once

#include "Platform/Vulkan/VulkanPhysicalDevice.h"

namespace Eppo
{
	class VulkanLogicalDevice
	{
	public:
		VulkanLogicalDevice(Ref<VulkanPhysicalDevice> physicalDevice);
		virtual ~VulkanLogicalDevice() = default;

		VkDevice GetNativeDevice() const { return m_Device; }

		Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }

		VkCommandBuffer GetCommandBuffer(bool begin) const;
		VkCommandBuffer GetSecondaryCommandBuffer() const;
		void FlushCommandBuffer(VkCommandBuffer commandBuffer) const;

	private:
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		VkDevice m_Device;

		VkQueue m_GraphicsQueue;
		VkCommandPool m_CommandPool;
	};
}
