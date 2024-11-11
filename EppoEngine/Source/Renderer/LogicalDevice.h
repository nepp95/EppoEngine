#pragma once

#include "Renderer/PhysicalDevice.h"

namespace Eppo
{
	class LogicalDevice
	{
	public:
		LogicalDevice(Ref<PhysicalDevice> physicalDevice);
		~LogicalDevice() = default;

		VkDevice GetNativeDevice() const { return m_Device; }

		Ref<PhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }

		VkCommandBuffer GetCommandBuffer(bool begin);
		VkCommandBuffer GetSecondaryCommandBuffer();
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);

	private:
		Ref<PhysicalDevice> m_PhysicalDevice;
		VkDevice m_Device;

		VkQueue m_GraphicsQueue;
		VkCommandPool m_CommandPool;
	};
}
