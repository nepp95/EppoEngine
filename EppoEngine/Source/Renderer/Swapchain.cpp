#include "pch.h"
#include "Swapchain.h"

#include "Renderer/RendererContext.h"

#include <glfw/glfw3.h>

namespace Eppo
{
	Swapchain::Swapchain()
	{
		EPPO_PROFILE_FUNCTION("Swapchain::Swapchain");

		Create();

		// Cleanup
		RendererContext::Get()->SubmitResourceFree([this]()
		{
			this->Cleanup();
			this->Destroy();
		});
	}

	void Swapchain::BeginFrame()
	{
		EPPO_PROFILE_FUNCTION("Swapchain::BeginFrame");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		{
			EPPO_PROFILE_FUNCTION("Swapchain::BeginFrame - Acquire Image");
			vkAcquireNextImageKHR(device, m_Swapchain, UINT64_MAX, m_PresentSemaphores[m_CurrentFrameIndex], VK_NULL_HANDLE, &m_CurrentImageIndex);
		}
		
		vkResetCommandPool(device, m_CommandPools[m_CurrentFrameIndex], 0);
	}

	void Swapchain::Present()
	{
		EPPO_PROFILE_FUNCTION("Swapchain::Present");

		Ref<LogicalDevice> logicalDevice = RendererContext::Get()->GetLogicalDevice();

		VkResult result;
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_PresentSemaphores[m_CurrentFrameIndex];
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrameIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_RenderSemaphores[m_CurrentFrameIndex];

		VK_CHECK(vkResetFences(logicalDevice->GetNativeDevice(), 1, &m_Fences[m_CurrentFrameIndex]), "Failed to reset fence!");
		VK_CHECK(vkQueueSubmit(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_Fences[m_CurrentFrameIndex]), "Failed to submit queue!");

		{
			EPPO_PROFILE_FUNCTION("Swapchain::Present - Queue Present");

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &m_RenderSemaphores[m_CurrentFrameIndex];
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &m_Swapchain;
			presentInfo.pImageIndices = &m_CurrentImageIndex;
			presentInfo.pResults = nullptr;

			result = vkQueuePresentKHR(logicalDevice->GetGraphicsQueue(), &presentInfo);
		}

		// Resize
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			OnResize();

		m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % VulkanConfig::MaxFramesInFlight;

		vkWaitForFences(logicalDevice->GetNativeDevice(), 1, &m_Fences[m_CurrentFrameIndex], VK_TRUE, UINT64_MAX);
	}

	void Swapchain::Cleanup()
	{
		EPPO_PROFILE_FUNCTION("Swapchain::Cleanup");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		for (auto& framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);

		for (auto& imageView : m_ImageViews)
			vkDestroyImageView(device, imageView, nullptr);
	}

	void Swapchain::Create(bool recreate)
	{
		Ref<RendererContext> context = RendererContext::Get();
		Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();

		if (!recreate)
			VK_CHECK(glfwCreateWindowSurface(context->GetVulkanInstance(), context->GetWindowHandle(), nullptr, &m_Surface), "Failed to create window surface!");

		SwapchainSupportDetails details = QuerySwapchainSupport(context);

		VkSurfaceFormatKHR format = ChooseSwapSurfaceFormat(details.Formats);
		m_ImageFormat = format.format;

		VkPresentModeKHR presentMode = ChooseSwapPresentMode(details.PresentModes);
		m_Extent = ChooseSwapExtent(details.Capabilities);

		uint32_t imageCount = details.Capabilities.minImageCount + 1;
		if (details.Capabilities.maxImageCount > 0 && imageCount > details.Capabilities.maxImageCount)
			imageCount = details.Capabilities.maxImageCount;


		// TODO: TEMP:
		imageCount = 2;

		VkSwapchainKHR oldSwapchain;
		if (recreate)
			oldSwapchain = m_Swapchain;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.preTransform = details.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = recreate ? oldSwapchain : VK_NULL_HANDLE;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = m_ImageFormat;
		createInfo.imageColorSpace = format.colorSpace;
		createInfo.imageExtent = m_Extent;
		createInfo.imageArrayLayers = 1;
		// TODO: VK_IMAGE_USAGE_TRANSFER_DST_BIT can be used when rendering to a seperate image instead of rendering to the swapchain image
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = logicalDevice->GetPhysicalDevice()->GetQueueFamilyIndices();
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.Graphics };

		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;

		VkDevice device = logicalDevice->GetNativeDevice();
		VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Swapchain), "Failed to create swapchain!");

		if (recreate)
			vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

		// Images
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		m_ImageCount = imageCount;
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_Images.data());

		// Image views
		m_ImageViews.resize(m_Images.size());

		for (size_t i = 0; i < m_Images.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_Images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_ImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &m_ImageViews[i]), "Failed to create image view!");
		}

		if (!recreate)
		{
			// Command pools and buffers
			VkCommandPoolCreateInfo commandPoolInfo{};
			commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			commandPoolInfo.queueFamilyIndex = indices.Graphics;

			m_CommandPools.resize(VulkanConfig::MaxFramesInFlight);
			m_CommandBuffers.resize(VulkanConfig::MaxFramesInFlight);

			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;

			for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			{
				VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &m_CommandPools[i]), "Failed to create command pool!");
				allocInfo.commandPool = m_CommandPools[i];
				VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &m_CommandBuffers[i]), "Failed to allocate command buffer!");
			}

			// Sync objects
			m_Fences.resize(VulkanConfig::MaxFramesInFlight);
			m_RenderSemaphores.resize(VulkanConfig::MaxFramesInFlight);
			m_PresentSemaphores.resize(VulkanConfig::MaxFramesInFlight);

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			{
				VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_RenderSemaphores[i]), "Failed to create semaphore!");
				VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_PresentSemaphores[i]), "Failed to create semaphore!");
				VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_Fences[i]), "Failed to create fence!");
			}

			// Render pass
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = m_ImageFormat;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpassDescription{};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorAttachmentRef;

			VkSubpassDependency subpassDependency{};
			subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpassDependency.dstSubpass = 0;
			subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.srcAccessMask = 0;
			subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &subpassDependency;

			VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass), "Failed to create render pass!");
		}

		// Framebuffers
		m_Framebuffers.resize(m_ImageViews.size());

		for (size_t i = 0; i < m_ImageViews.size(); i++)
		{
			VkImageView attachments[] = {
				m_ImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = GetWidth();
			framebufferInfo.height = GetHeight();
			framebufferInfo.layers = 1;

			VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffers[i]), "Failed to create framebuffer!");
		}
	}

	void Swapchain::Destroy()
	{
		EPPO_PROFILE_FUNCTION("Swapchain::Destroy");

		VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();

		vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
		vkDestroyRenderPass(device, m_RenderPass, nullptr);

		for (auto& semaphore : m_PresentSemaphores)
			vkDestroySemaphore(device, semaphore, nullptr);

		for (auto& semaphore : m_RenderSemaphores)
			vkDestroySemaphore(device, semaphore, nullptr);

		for (auto& fence : m_Fences)
			vkDestroyFence(device, fence, nullptr);

		for (auto& commandPool : m_CommandPools)
			vkDestroyCommandPool(device, commandPool, nullptr);

		VkInstance instance = RendererContext::Get()->GetVulkanInstance();
		vkDestroySurfaceKHR(instance, m_Surface, nullptr);
	}

	void Swapchain::OnResize()
	{
		EPPO_PROFILE_FUNCTION("Swapchain::OnResize");

		RendererContext::Get()->WaitIdle();
		Cleanup();
		Create(true);
		RendererContext::Get()->WaitIdle();
	}

	SwapchainSupportDetails Swapchain::QuerySwapchainSupport(Ref<RendererContext> context)
	{
		EPPO_PROFILE_FUNCTION("Swapchain::QuerySwapchainSupport");

		VkPhysicalDevice physicalDevice = context->GetLogicalDevice()->GetPhysicalDevice()->GetNativeDevice();

		SwapchainSupportDetails details;

		// Capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &details.Capabilities);

		// Formats
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);

		if (formatCount > 0)
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, details.Formats.data());
		}

		// Presentation modes
		uint32_t presentationModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentationModeCount, nullptr);

		if (presentationModeCount > 0)
		{
			details.PresentModes.resize(presentationModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentationModeCount, details.PresentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		EPPO_PROFILE_FUNCTION("Swapchain::ChooseSwapSurfaceFormat");

		for (const auto& format : availableFormats)
		{
			if (format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}

		return availableFormats[0];
	}

	VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		EPPO_PROFILE_FUNCTION("Swapchain::ChooseSwapPresentMode");

		for (const auto& presentMode : availablePresentModes)
		{
			//if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			if (presentMode == VK_PRESENT_MODE_FIFO_KHR)
				return presentMode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		EPPO_PROFILE_FUNCTION("Swapchain::ChooseSwapExtent");

		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;

		int width = 0, height = 0;
		glfwGetFramebufferSize(RendererContext::Get()->GetWindowHandle(), &width, &height);

		VkExtent2D actualExtent = { (uint32_t)width, (uint32_t)height };

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}
