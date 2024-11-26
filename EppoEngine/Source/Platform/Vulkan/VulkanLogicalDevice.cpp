#include "pch.h"
#include "VulkanLogicalDevice.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
	VulkanLogicalDevice::VulkanLogicalDevice(Ref<VulkanPhysicalDevice> physicalDevice)
		: m_PhysicalDevice(physicalDevice)
	{
		// Check if requested extensions are supported by the GPU
		bool extensionsSupported = true;
		for (const auto& extension : VulkanConfig::DeviceExtensions)
		{
			if (!m_PhysicalDevice->IsExtensionSupported(extension))
				extensionsSupported = false;
		}
		EPPO_ASSERT(extensionsSupported);

		// Create device queue
		QueueFamilyIndices indices = m_PhysicalDevice->GetQueueFamilyIndices();
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
		deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		deviceQueueCreateInfo.queueFamilyIndex = indices.Graphics;
		deviceQueueCreateInfo.queueCount = 1;
		deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(deviceQueueCreateInfo);

		VkPhysicalDeviceFeatures deviceFeatures = m_PhysicalDevice->GetDeviceFeatures();

		///// ENABLE FEATURES HERE
		VkPhysicalDeviceSynchronization2Features syncFeatures{};
		syncFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		syncFeatures.synchronization2 = VK_TRUE;

		VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
		descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		descriptorIndexingFeatures.pNext = &syncFeatures;

		VkPhysicalDeviceMultiviewFeatures multiviewFeatures{};
		multiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
		multiviewFeatures.multiview = VK_TRUE;
		multiviewFeatures.pNext = &descriptorIndexingFeatures;

		VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
		dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
		dynamicRenderingFeatures.pNext = &multiviewFeatures;
		/////

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(VulkanConfig::DeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = VulkanConfig::DeviceExtensions.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.pNext = &dynamicRenderingFeatures;

		if (VulkanConfig::EnableValidation)
		{
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VulkanConfig::ValidationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = VulkanConfig::ValidationLayers.data();
		}
		else
		{
			deviceCreateInfo.enabledLayerCount = 0;
			deviceCreateInfo.ppEnabledLayerNames = nullptr;
		}

		VK_CHECK(vkCreateDevice(physicalDevice->GetNativeDevice(), &deviceCreateInfo, nullptr, &m_Device), "Failed to create logical device!");

		// Device queue
		vkGetDeviceQueue(m_Device, indices.Graphics, 0, &m_GraphicsQueue);

		// Create command pool
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = indices.Graphics;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "Failed to create command pool!");

		// Clean up
		Ref<VulkanContext> context = VulkanContext::Get();
		context->SubmitResourceFree([this]()
		{
			EPPO_WARN("Releasing logical device and command pool {}", (void*)this);
			vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

			vkDeviceWaitIdle(m_Device);
			vkDestroyDevice(m_Device, nullptr);
		});
	}

	VkCommandBuffer VulkanLogicalDevice::GetCommandBuffer(bool begin) const
	{
		EPPO_PROFILE_FUNCTION("VulkanLogicalDevice::GetCommandBuffer");

		VkCommandBuffer commandBuffer;

		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = m_CommandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;

		VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocateInfo, &commandBuffer), "Failed to allocate command buffer!");

		if (begin)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer!");
		}

		return commandBuffer;
	}

	VkCommandBuffer VulkanLogicalDevice::GetSecondaryCommandBuffer() const
	{
		EPPO_PROFILE_FUNCTION("VulkanLogicalDevice::GetSecondaryCommandBuffer");

		VkCommandBuffer commandBuffer;

		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = m_CommandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocateInfo.commandBufferCount = 1;

		VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocateInfo, &commandBuffer), "Failed to allocate command buffer!");

		return commandBuffer;
	}

	void VulkanLogicalDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer) const
	{
		EPPO_PROFILE_FUNCTION("VulkanLogicalDevice::FlushCommandBuffer");

		VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to end command buffer!");

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// We need this work to be done when this function ends so we synchronize it
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0;

		VkFence fence;
		VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &fence), "Failed to create fence!");

		VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence), "Failed to submit work to queue!");
		vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX);

		// Clean up
		vkDestroyFence(m_Device, fence, nullptr);
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}

	void VulkanLogicalDevice::FreeCommandBuffer(VkCommandBuffer commandBuffer) const
	{
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
	}
}
