#pragma once

#include "Platform/Vulkan/VulkanPhysicalDevice.h"

namespace Eppo
{
	class VulkanLogicalDevice
	{
	public:
		explicit VulkanLogicalDevice(const Ref<VulkanPhysicalDevice>& physicalDevice);
		virtual ~VulkanLogicalDevice() = default;

		[[nodiscard]] VkDevice GetNativeDevice() const { return m_Device; }

		[[nodiscard]] Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		[[nodiscard]] VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }

		[[nodiscard]] VkCommandBuffer GetCommandBuffer(bool begin) const;
		[[nodiscard]] VkCommandBuffer GetSecondaryCommandBuffer() const;
		void FlushCommandBuffer(VkCommandBuffer commandBuffer) const;
		void FreeCommandBuffer(VkCommandBuffer commandBuffer) const;

	private:
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		VkDevice m_Device;

		VkQueue m_GraphicsQueue;
		VkCommandPool m_CommandPool;
	};
}
