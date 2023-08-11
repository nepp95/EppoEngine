#include "pch.h"
#include "LogicalDevice.h"

#include "Renderer/RendererContext.h"

namespace Eppo
{
	LogicalDevice::LogicalDevice(Ref<PhysicalDevice> physicalDevice)
		: m_PhysicalDevice(physicalDevice)
	{
		EPPO_PROFILE_FUNCTION("LogicalDevice::LogicalDevice");

		// Check if all required extensions are supported by the device
		bool extensionSupported = true;
		for (const auto& extension : VulkanConfig::DeviceExtensions)
			if (!m_PhysicalDevice->IsExtensionSupported(extension))
				extensionSupported = false;

		EPPO_ASSERT(extensionSupported);

		// Create logical device
		QueueFamilyIndices indices = m_PhysicalDevice->GetQueueFamilyIndices();
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo graphicsQueueCreateInfo{};
		graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphicsQueueCreateInfo.queueFamilyIndex = indices.Graphics;
		graphicsQueueCreateInfo.queueCount = 1;
		graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(graphicsQueueCreateInfo);

		VkPhysicalDeviceFeatures deviceFeatures = m_PhysicalDevice->GetDeviceFeatures();

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = (uint32_t)VulkanConfig::DeviceExtensions.size();
		createInfo.ppEnabledExtensionNames = VulkanConfig::DeviceExtensions.data();
		if (VulkanConfig::EnableValidation)
		{
			createInfo.enabledLayerCount = (uint32_t)VulkanConfig::ValidationLayers.size();
			createInfo.ppEnabledLayerNames = VulkanConfig::ValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VK_CHECK(vkCreateDevice(m_PhysicalDevice->GetNativeDevice(), &createInfo, nullptr, &m_Device), "Failed to create logical device!");
		vkGetDeviceQueue(m_Device, indices.Graphics, 0, &m_GraphicsQueue);

		// Create command pool
		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.queueFamilyIndex = indices.Graphics;
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool), "Failed to create command pool!");
	
		// Clean up
		Ref<RendererContext> context = RendererContext::Get();
		context->SubmitResourceFree([=]()
		{
			vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
			vkDestroyDevice(m_Device, nullptr);
		});
	}

	VkCommandBuffer LogicalDevice::GetCommandBuffer(bool begin)
	{
		EPPO_PROFILE_FUNCTION("LogicalDevice::GetCommandBuffer");

		VkCommandBuffer commandBuffer;

		VkCommandBufferAllocateInfo commandBufferInfo{};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.commandPool = m_CommandPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = 1;

		VK_CHECK(vkAllocateCommandBuffers(m_Device, &commandBufferInfo, &commandBuffer), "Failed to allocate command buffer!");

		if (begin)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer!");
		}

		return commandBuffer;
	}

	void LogicalDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer)
	{
		EPPO_PROFILE_FUNCTION("LogicalDevice::FlushCommandBuffer");

		VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer!");

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0;

		// We need this to be done when this function end so we sync it
		VkFence fence;
		VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &fence), "Failed to create temporary fence!");

		// Submit work
		VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence), "Failed to submit queue!");
		vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX);

		// Clean up
		vkDestroyFence(m_Device, fence, nullptr);
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}
}
