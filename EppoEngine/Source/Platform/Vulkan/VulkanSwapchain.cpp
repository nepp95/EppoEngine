#include "pch.h"
#include "VulkanSwapchain.h"

#include "Core/Application.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanImage.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	VulkanSwapchain::VulkanSwapchain(Ref<VulkanLogicalDevice> logicalDevice)
		: m_LogicalDevice(logicalDevice)
	{
		Create();
	}

	void VulkanSwapchain::Create(bool recreate)
	{
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();

		// Swapchain support details
		SwapchainSupportDetails details = QuerySwapchainSupportDetails(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = SelectSurfaceFormat(details.Formats);
		m_ImageFormat = surfaceFormat.format;

		VkPresentModeKHR presentMode = SelectPresentMode(details.PresentModes);
		m_Extent = SelectExtent(details.Capabilities);

		// Image count
		uint32_t imageCount = details.Capabilities.minImageCount + 1;
		if (details.Capabilities.maxImageCount > 0 && imageCount > details.Capabilities.maxImageCount)
			imageCount = details.Capabilities.maxImageCount;

		// TODO: Remove this and use a dynamic amount of images
		imageCount = VulkanConfig::MaxFramesInFlight;

		// Create swapchain
		VkSwapchainKHR oldSwapchain;
		if (recreate)
			oldSwapchain = m_Swapchain;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = physicalDevice->GetSurface();
		createInfo.preTransform = details.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = recreate ? oldSwapchain : VK_NULL_HANDLE;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = m_ImageFormat;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = m_Extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		QueueFamilyIndices indices = physicalDevice->GetQueueFamilyIndices();
		std::array<uint32_t, 2> queueFamilies = { static_cast<uint32_t>(indices.Graphics), static_cast<uint32_t>(indices.Present) };

		if (indices.Graphics != indices.Present)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size());
			createInfo.pQueueFamilyIndices = queueFamilies.data();
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		VkDevice device = m_LogicalDevice->GetNativeDevice();
		VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Swapchain), "Failed to create swapchain!");

		if (recreate)
			vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

		// Get swapchain images
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_Images.data());

		// Create image views
		VkCommandBuffer cmd = m_LogicalDevice->GetCommandBuffer(true);
		m_ImageViews.resize(imageCount);

		for (uint32_t i = 0; i < imageCount; i++)
		{
			VkImageViewCreateInfo imageViewCreateInfo{};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = m_Images[i];
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = m_ImageFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;

			VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_ImageViews[i]), "Failed to create image view!");

			// Transition images to present layout
			VulkanImage::TransitionImage(cmd, m_Images[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		m_LogicalDevice->FlushCommandBuffer(cmd);

		if (!recreate)
		{
			// These things do not need to be recreated upon swapchain recreation
			m_CommandBuffer = CreateRef<VulkanCommandBuffer>(false, 0);

			// Sync objects
			m_Fences.resize(VulkanConfig::MaxFramesInFlight);
			m_PresentSemaphores.resize(VulkanConfig::MaxFramesInFlight);
			m_RenderSemaphores.resize(VulkanConfig::MaxFramesInFlight);

			VkFenceCreateInfo fenceCreateInfo{};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			VkSemaphoreCreateInfo semaphoreCreateInfo{};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			{
				VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &m_RenderSemaphores[i]), "Failed to create semaphore!")
					VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &m_PresentSemaphores[i]), "Failed to create semaphore!")
					VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &m_Fences[i]), "Failed to create fence!")
			}
		}

		context->SubmitResourceFree([this]()
		{
			EPPO_WARN("Releasing swapchain {}", (void*)this);
			Destroy();
		});
	}

	void VulkanSwapchain::Cleanup()
	{

	}

	void VulkanSwapchain::Destroy()
	{
		VkDevice device = m_LogicalDevice->GetNativeDevice();

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			EPPO_WARN("[Swapchain] Releasing semaphore {}", (void*)m_PresentSemaphores[i]);
			vkDestroySemaphore(device, m_PresentSemaphores[i], nullptr);

			EPPO_WARN("[Swapchain] Releasing semaphore {}", (void*)m_RenderSemaphores[i]);
			vkDestroySemaphore(device, m_RenderSemaphores[i], nullptr);

			EPPO_WARN("[Swapchain] Releasing fence {}", (void*)m_Fences[i]);
			vkDestroyFence(device, m_Fences[i], nullptr);

			EPPO_WARN("[Swapchain] Releasing image view {}", (void*)m_ImageViews[i]);
			vkDestroyImageView(device, m_ImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
	}

	void VulkanSwapchain::BeginFrame()
	{
		EPPO_PROFILE_FUNCTION("VulkanSwapchain::BeginFrame");

		VkDevice device = m_LogicalDevice->GetNativeDevice();

		vkAcquireNextImageKHR(device, m_Swapchain, UINT64_MAX, m_PresentSemaphores[m_CurrentFrameIndex], VK_NULL_HANDLE, &m_CurrentImageIndex);
		m_CommandBuffer->ResetCommandBuffer(m_CurrentFrameIndex);
	}

	void VulkanSwapchain::PresentFrame()
	{
		EPPO_PROFILE_FUNCTION("VulkanSwapchain::PresentFrame");

		VkResult result;
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkCommandBuffer commandBuffer = m_CommandBuffer->GetCurrentCommandBuffer();

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_PresentSemaphores[m_CurrentFrameIndex];
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_RenderSemaphores[m_CurrentFrameIndex];

		VK_CHECK(vkResetFences(m_LogicalDevice->GetNativeDevice(), 1, &m_Fences[m_CurrentFrameIndex]), "Failed to reset fence!");
		VK_CHECK(vkQueueSubmit(m_LogicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_Fences[m_CurrentFrameIndex]), "Failed to submit work to queue!");

		Ref<VulkanContext> context = VulkanContext::Get();
		EPPO_PROFILE_GPU_END(context->GetTracyContext(), commandBuffer);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_RenderSemaphores[m_CurrentFrameIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Swapchain;
		presentInfo.pImageIndices = &m_CurrentImageIndex;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(m_LogicalDevice->GetGraphicsQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			OnResize();

		m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % VulkanConfig::MaxFramesInFlight;

		// TODO: Maybe do this elsewhere?
		m_FrameCounter++;
		VulkanContext::Get()->RunGC(m_FrameCounter);

		vkWaitForFences(m_LogicalDevice->GetNativeDevice(), 1, &m_Fences[m_CurrentFrameIndex], VK_TRUE, UINT64_MAX);
	}

	void VulkanSwapchain::OnResize()
	{
		// TODO: Swapchain::OnResize
		EPPO_ASSERT(false);
	}

	SwapchainSupportDetails VulkanSwapchain::QuerySwapchainSupportDetails(const Ref<VulkanPhysicalDevice>& physicalDevice)
	{
		SwapchainSupportDetails details;
		VkPhysicalDevice device = physicalDevice->GetNativeDevice();

		auto* surface = m_LogicalDevice->GetPhysicalDevice()->GetSurface();

		// Capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

		// Formats
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());
		}

		// Presentation modes
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR VulkanSwapchain::SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
	{
		VkSurfaceFormatKHR surfaceFormat{};

		for (const auto& format : surfaceFormats)
		{
			if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = format;
				break;
			}
		}

		if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
		{
			EPPO_WARN("Undefined swapchain format!");
			surfaceFormat = surfaceFormats.back();
		}

		return surfaceFormat;
	}

	VkPresentModeKHR VulkanSwapchain::SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
	{
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // FIFO_KHR is required to be supported so will always be valid

		for (const auto& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				presentMode = mode;
				break;
			}
		}

		return presentMode;
	}

	VkExtent2D VulkanSwapchain::SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		VkExtent2D extent = capabilities.currentExtent;

		if (capabilities.currentExtent.width == UINT32_MAX)
		{
			int width = 0;
			int height = 0;
			glfwGetFramebufferSize(Application::Get().GetWindow().GetNativeWindow(), &width, &height);

			extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

			extent.width = std::clamp(extent.width, capabilities.maxImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(extent.height, capabilities.maxImageExtent.height, capabilities.maxImageExtent.height);
		}

		return extent;
	}
}
