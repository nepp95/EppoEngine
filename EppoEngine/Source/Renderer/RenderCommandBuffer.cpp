#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	RenderCommandBuffer::RenderCommandBuffer(bool manualSubmission, uint32_t count)
		: m_ManualSubmission(manualSubmission)
	{
		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		// Create command pool
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = context->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "Failed to create command pool!");

		// Allocate command buffers
		if (count == 0)
			count = VulkanConfig::MaxFramesInFlight;

		m_CommandBuffers.resize(count);

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = m_CommandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = count;

		VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, m_CommandBuffers.data()), "Failed to allocate command buffers!");

		// Create fences
		m_Fences.resize(count);

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < m_Fences.size(); i++)
			VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &m_Fences[i]), "Failed to create fences!");
	}

	RenderCommandBuffer::~RenderCommandBuffer()
	{
		Ref<RendererContext> context = RendererContext::Get();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			vkDestroyFence(device, m_Fences[i], nullptr);

		vkDestroyCommandPool(device, m_CommandPool, nullptr);
	}

	void RenderCommandBuffer::RT_Begin()
	{
		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

			VkCommandBufferBeginInfo commandBufferBeginInfo{};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			commandBufferBeginInfo.pNext = nullptr;

			VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[frameIndex], &commandBufferBeginInfo), "Failed to begin command buffer!");
		});
	}

	void RenderCommandBuffer::RT_End()
	{
		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

			VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[frameIndex]), "Failed to end command buffer!");
		});
	}

	void RenderCommandBuffer::RT_Submit()
	{
		if (!m_ManualSubmission)
		{
			EPPO_WARN("Trying to manually submit command buffer on a command buffer that is submitted automatically!");
			return;
		}

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();
			VkDevice device = logicalDevice->GetNativeDevice();
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_CommandBuffers[frameIndex];

			VK_CHECK(vkResetFences(device, 1, &m_Fences[frameIndex]), "Failed to reset fence!");
			VK_CHECK(vkQueueSubmit(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_Fences[frameIndex]), "Failed to submit work to queue!");
			VK_CHECK(vkWaitForFences(device, 1, &m_Fences[frameIndex], VK_TRUE, UINT64_MAX), "Failed to wait for fence!");
		});
	}

	void RenderCommandBuffer::ResetCommandBuffer(uint32_t frameIndex)
	{
		vkResetCommandBuffer(m_CommandBuffers[frameIndex], 0);
	}

	VkCommandBuffer RenderCommandBuffer::GetCurrentCommandBuffer()
	{
		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();
		return m_CommandBuffers[imageIndex];
	}
}
