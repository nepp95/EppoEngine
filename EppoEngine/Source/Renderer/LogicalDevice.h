#pragma once

#include "Renderer/PhysicalDevice.h"

namespace Eppo
{
	class LogicalDevice
	{
	public:
		LogicalDevice(Ref<PhysicalDevice> physicalDevice);

		VkCommandBuffer GetCommandBuffer(bool begin);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);
		VkCommandBuffer GetSecondaryCommandBuffer();

		const Ref<PhysicalDevice>& GetPhysicalDevice() const { return m_PhysicalDevice; }
		const VkDevice& GetNativeDevice() const { return m_Device; }
		const VkQueue& GetGraphicsQueue() const { return m_GraphicsQueue; }

	private:
		Ref<PhysicalDevice> m_PhysicalDevice;

		VkDevice m_Device;
		VkQueue m_GraphicsQueue;
		VkCommandPool m_CommandPool;
	};
}
