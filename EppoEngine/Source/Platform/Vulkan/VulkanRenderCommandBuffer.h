#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/RenderCommandBuffer.h"

namespace Eppo
{
	class VulkanRenderCommandBuffer : public RenderCommandBuffer
	{
	public:
		VulkanRenderCommandBuffer(uint32_t count);
		virtual ~VulkanRenderCommandBuffer();

		void Begin() override;
		void End() override;
		void Submit() override;

		uint32_t BeginTimestampQuery() override;
		void EndTimestampQuery(uint32_t queryIndex) override;

		float GetTimestamp(uint32_t imageIndex, uint32_t queryIndex) const override;
		const PipelineStatistics& GetPipelineStatistics(uint32_t imageIndex) const override;

		VkCommandBuffer GetCurrentCommandBuffer() const;

	private:
		VkCommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;

		std::vector<VkQueryPool> m_QueryPools;
		std::vector<std::vector<uint64_t>> m_Timestamps;
		std::vector<std::vector<float>> m_TimestampDeltas;
		uint32_t m_QueryIndex = 2;
		uint32_t m_QueryCount = 6;

		std::vector<VkQueryPool> m_PipelineQueryPools;
		std::vector<PipelineStatistics> m_PipelineStatistics;
		uint32_t m_PipelineQueryCount = 6;

		std::vector<VkFence> m_Fences;
	};
}
