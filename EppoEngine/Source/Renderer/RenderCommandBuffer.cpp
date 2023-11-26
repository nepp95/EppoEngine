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

		// Timestamp queries
		m_QueryPools.resize(VulkanConfig::MaxFramesInFlight);

		VkQueryPoolCreateInfo queryPoolInfo{};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryPoolInfo.queryCount = m_QueryCount;

		for (size_t i = 0; i < m_QueryPools.size(); i++)
			VK_CHECK(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &m_QueryPools[i]), "Failed to create query pool!");

		m_Timestamps.resize(VulkanConfig::MaxFramesInFlight);
		for (auto& timestamp : m_Timestamps)
			timestamp.resize(m_QueryCount);

		m_TimestampDeltas.resize(VulkanConfig::MaxFramesInFlight);
		for (auto& timestamp : m_TimestampDeltas)
			timestamp.resize(m_QueryCount / 2);

		// Pipeline queries
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

		for (size_t i = 0; i < m_PipelineQueryPools.size(); i++)
			VK_CHECK(vkCreateQueryPool(device, &queryPoolInfo, nullptr, &m_PipelineQueryPools[i]), "Failed to create query pool!");

		m_PipelineStatistics.resize(VulkanConfig::MaxFramesInFlight);
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

		m_QueryIndex = 2;

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pNext = nullptr;

			VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[imageIndex], &beginInfo), "Failed to begin command buffer!");

			vkCmdResetQueryPool(m_CommandBuffers[imageIndex], m_QueryPools[imageIndex], 0, m_QueryCount);
			vkCmdWriteTimestamp(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_QueryPools[imageIndex], 0);

			vkCmdResetQueryPool(m_CommandBuffers[imageIndex], m_PipelineQueryPools[imageIndex], 0, m_PipelineQueryCount);
			vkCmdBeginQuery(m_CommandBuffers[imageIndex], m_PipelineQueryPools[imageIndex], 0, 0);
		});
	}

	void RenderCommandBuffer::End()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::End");

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			vkCmdWriteTimestamp(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[imageIndex], 1);
			vkCmdEndQuery(m_CommandBuffers[imageIndex], m_PipelineQueryPools[imageIndex], 0);

			VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[imageIndex]), "Failed to end command buffer!");
		});
	}

	void RenderCommandBuffer::Submit()
	{
		EPPO_PROFILE_FUNCTION("RenderCommandBuffer::Submit");

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<LogicalDevice> logicalDevice = context->GetLogicalDevice();
			Ref<PhysicalDevice> physicalDevice = context->GetPhysicalDevice();
			VkDevice device = logicalDevice->GetNativeDevice();

			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_CommandBuffers[imageIndex];

			VK_CHECK(vkResetFences(device, 1, &m_Fences[imageIndex]), "Failed to reset fence!");
			VK_CHECK(vkQueueSubmit(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, m_Fences[imageIndex]), "Failed to submit queue!");
			VK_CHECK(vkWaitForFences(device, 1, &m_Fences[imageIndex], VK_TRUE, UINT64_MAX), "Failed to wait for fence!");

			vkGetQueryPoolResults(device, m_QueryPools[imageIndex], 0, m_QueryCount, sizeof(uint64_t) * m_QueryCount, m_Timestamps[imageIndex].data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

			for (uint32_t i = 0; i < m_QueryIndex; i += 2)
			{
				uint64_t begin = m_Timestamps[imageIndex][i];
				uint64_t end = m_Timestamps[imageIndex][i + 1];

				float delta = 0.0f;
				if (end > begin)
					delta = (end - begin) * physicalDevice->GetDeviceProperties().limits.timestampPeriod * 0.000001f; // Time was originally in nanoseconds

				m_TimestampDeltas[imageIndex][i / 2] = delta;
			}

			vkGetQueryPoolResults(device, m_PipelineQueryPools[imageIndex], 0, 1, sizeof(PipelineStatistics), &m_PipelineStatistics[imageIndex], sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
		});
	}

	uint32_t RenderCommandBuffer::BeginTimestampQuery()
	{
		uint32_t queryIndex = m_QueryIndex;
		m_QueryIndex += 2;

		Renderer::SubmitCommand([this, queryIndex]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			vkCmdWriteTimestamp(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_QueryPools[imageIndex], queryIndex);
		});

		return queryIndex;
	}

	void RenderCommandBuffer::EndTimestampQuery(uint32_t queryIndex)
	{
		Renderer::SubmitCommand([this, queryIndex]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

			vkCmdWriteTimestamp(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[imageIndex], queryIndex + 1);
		});
	}

	float RenderCommandBuffer::GetTimestamp(uint32_t imageIndex, uint32_t queryIndex) const
	{
		const auto& timing = m_TimestampDeltas[imageIndex];

		return timing[queryIndex / 2];
	}

	VkCommandBuffer RenderCommandBuffer::GetCurrentCommandBuffer() const
	{
		uint32_t imageIndex = RendererContext::Get()->GetSwapchain()->GetCurrentImageIndex();

		return m_CommandBuffers[imageIndex];
	}
}
