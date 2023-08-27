#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	RenderCommandBuffer::RenderCommandBuffer(uint32_t count)
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::RenderCommandBuffer");

		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		// Create command pool
		VkCommandPoolCreateInfo cmdPoolInfo{};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = context->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &m_CommandPool), "Failed to create command pool!");

		// Allocate command buffers
		if (count == 0)
			count = VulkanConfig::MaxFramesInFlight;
		m_CommandBuffers.resize(count);

		VkCommandBufferAllocateInfo cmdAllocInfo{};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.commandPool = m_CommandPool;
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAllocInfo.commandBufferCount = count;
		
		VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, m_CommandBuffers.data()), "Failed to allocate command buffers!");

		// Create fences
		m_Fences.resize(VulkanConfig::MaxFramesInFlight);
		
		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < m_Fences.size(); i++)
			VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &m_Fences[i]), "Failed to create fence!");
	}

	RenderCommandBuffer::~RenderCommandBuffer()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::~RenderCommandBuffer");

		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			vkDestroyFence(device, m_Fences[i], nullptr);

		vkDestroyCommandPool(device, m_CommandPool, nullptr);
	}

	void RenderCommandBuffer::Begin()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::Begin");

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			uint32_t imageIndex = swapchain->GetCurrentImageIndex();

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pNext = nullptr;

			VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[imageIndex], &beginInfo), "Failed to begin command buffer!");
		});
	}

	void RenderCommandBuffer::End()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::End");

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();

			uint32_t imageIndex = swapchain->GetCurrentImageIndex();

			VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[imageIndex]), "Failed to end command buffer!");
		});
	}

	void RenderCommandBuffer::Submit()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::Submit");

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();
			VkDevice device = logicalDevice->GetNativeDevice();

			uint32_t imageIndex = swapchain->GetCurrentImageIndex();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_CommandBuffers[imageIndex];

			VK_CHECK(vkResetFences(device, 1, &m_Fences[imageIndex]), "Failed to reset fence!");
			VK_CHECK(vkQueueSubmit(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_Fences[imageIndex]), "Failed to submit queue!");
			VK_CHECK(vkWaitForFences(device, 1, &m_Fences[imageIndex], VK_TRUE, UINT64_MAX), "Failed to wait for fence!");
		});
	}

	VkCommandBuffer RenderCommandBuffer::GetCurrentCommandBuffer() const
	{
		uint32_t imageIndex = RendererContext::Get()->GetSwapchain()->GetCurrentImageIndex();

		return m_CommandBuffers[imageIndex];
	}
}
