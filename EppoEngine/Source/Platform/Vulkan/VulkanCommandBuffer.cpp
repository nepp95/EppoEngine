#include "pch.h"
#include "VulkanCommandBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	VulkanCommandBuffer::VulkanCommandBuffer(bool manualSubmission, uint32_t count)
		: m_ManualSubmission(manualSubmission)
	{
		Ref<VulkanContext> context = VulkanContext::Get();
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

		for (auto& fence : m_Fences)
			VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence), "Failed to create fences!");

		// Queries
		m_QueryPools.resize(VulkanConfig::MaxFramesInFlight);

		VkQueryPoolCreateInfo queryPoolInfo{};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolInfo.queryCount = m_QueryCount;

		for (auto& queryPool : m_QueryPools)
			VK_CHECK(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool), "Failed to create query pool!");

		m_Timestamps.resize(VulkanConfig::MaxFramesInFlight);
		for (auto& timestamp : m_Timestamps)
			timestamp.resize(m_QueryCount);

		m_TimestampDeltas.resize(VulkanConfig::MaxFramesInFlight);
		for (auto& timestamp : m_TimestampDeltas)
			timestamp.resize(m_QueryCount / 2);

		m_PipelineQueryPools.resize(VulkanConfig::MaxFramesInFlight);
		queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		queryPoolInfo.queryCount = m_PipelineQueryCount;
		queryPoolInfo.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

		for (auto& pipelineQueryPool : m_PipelineQueryPools)
			VK_CHECK(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &pipelineQueryPool), "Failed to create query pool!");

		m_PipelineStatistics.resize(VulkanConfig::MaxFramesInFlight);

		// Cleanup
		context->SubmitResourceFree([this]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

			for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
			{
				vkDestroyQueryPool(device, m_QueryPools[i], nullptr);
				vkDestroyQueryPool(device, m_PipelineQueryPools[i], nullptr);
				vkDestroyFence(device, m_Fences[i], nullptr);
			}

			vkDestroyCommandPool(device, m_CommandPool, nullptr);
		});
	}

	void VulkanCommandBuffer::RT_Begin()
	{
		m_QueryIndex = 2;

		Renderer::SubmitCommand([this]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

			VkCommandBufferBeginInfo commandBufferBeginInfo{};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			commandBufferBeginInfo.pNext = nullptr;

			VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[frameIndex], &commandBufferBeginInfo), "Failed to begin command buffer!");

			vkCmdResetQueryPool(m_CommandBuffers[frameIndex], m_QueryPools[frameIndex], 0, m_QueryCount);
			vkCmdWriteTimestamp(m_CommandBuffers[frameIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_QueryPools[frameIndex], 0);

			vkCmdResetQueryPool(m_CommandBuffers[frameIndex], m_PipelineQueryPools[frameIndex], 0, m_PipelineQueryCount);
			vkCmdBeginQuery(m_CommandBuffers[frameIndex], m_PipelineQueryPools[frameIndex], 0, 0);
		});
	}

	void VulkanCommandBuffer::RT_End()
	{
		Renderer::SubmitCommand([this]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

			vkCmdWriteTimestamp(m_CommandBuffers[frameIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[frameIndex], 1);
			vkCmdEndQuery(m_CommandBuffers[frameIndex], m_PipelineQueryPools[frameIndex], 0);

			VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[frameIndex]), "Failed to end command buffer!");

			// Statistics
			Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();
			VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
			vkGetQueryPoolResults(device, m_QueryPools[frameIndex], 0, m_QueryCount, sizeof(uint64_t) * m_QueryCount, m_Timestamps[frameIndex].data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

			for (uint32_t i = 0; i < m_QueryIndex; i += 2)
			{
				uint64_t begin = m_Timestamps[frameIndex][i];
				uint64_t end = m_Timestamps[frameIndex][i + 1];

				float delta = 0.0f;
				if (end > begin)
					delta = (end - begin) * physicalDevice->GetDeviceProperties().limits.timestampPeriod * 0.000001f;

				m_TimestampDeltas[frameIndex][i / 2] = delta;
			}

			vkGetQueryPoolResults(device, m_PipelineQueryPools[frameIndex], 0, 1, sizeof(PipelineStatistics), &m_PipelineStatistics[frameIndex], sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
		});
	}

	void VulkanCommandBuffer::RT_Submit() const
	{
		if (!m_ManualSubmission)
		{
			EPPO_WARN("Trying to manually submit command buffer on a command buffer that is submitted automatically!");
			return;
		}

		Renderer::SubmitCommand([this]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			Ref<VulkanLogicalDevice> logicalDevice = context->GetLogicalDevice();
			VkDevice device = logicalDevice->GetNativeDevice();
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

			VkCommandBufferSubmitInfo cmdSubmitInfo{};
			cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmdSubmitInfo.commandBuffer = m_CommandBuffers[frameIndex];

			VkSubmitInfo2 submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
			submitInfo.pCommandBufferInfos = &cmdSubmitInfo;

			VK_CHECK(vkResetFences(device, 1, &m_Fences[frameIndex]), "Failed to reset fence!");
			VK_CHECK(vkQueueSubmit2(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_Fences[frameIndex]), "Failed to submit work to queue!");
			VK_CHECK(vkWaitForFences(device, 1, &m_Fences[frameIndex], VK_TRUE, UINT64_MAX), "Failed to wait for fence!");
		});
	}

	uint32_t VulkanCommandBuffer::RT_BeginTimestampQuery()
	{
		uint32_t queryIndex = m_QueryIndex;
		m_QueryIndex += 2;

		Renderer::SubmitCommand([this, queryIndex]()
		{
			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			vkCmdWriteTimestamp(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_QueryPools[imageIndex], queryIndex);
		});

		return queryIndex;
	}

	void VulkanCommandBuffer::RT_EndTimestampQuery(uint32_t queryIndex)
	{
		Renderer::SubmitCommand([this, queryIndex]()
		{
			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			vkCmdWriteTimestamp(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[imageIndex], queryIndex + 1);
		});
	}

	float VulkanCommandBuffer::GetTimestamp(uint32_t frameIndex, uint32_t queryIndex) const
	{
		const auto& timing = m_TimestampDeltas[frameIndex];

		return timing[queryIndex / 2];
	}

	void VulkanCommandBuffer::ResetCommandBuffer(uint32_t frameIndex)
	{
		vkResetCommandBuffer(m_CommandBuffers[frameIndex], 0);
	}

	VkCommandBuffer VulkanCommandBuffer::GetCurrentCommandBuffer()
	{
		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();
		return m_CommandBuffers[imageIndex];
	}
}
