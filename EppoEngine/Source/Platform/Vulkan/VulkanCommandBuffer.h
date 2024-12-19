#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/CommandBuffer.h"

namespace Eppo
{
	struct PipelineStatistics
	{
		uint64_t InputAssemblyVertices = 0;
		uint64_t InputAssemblyPrimitives = 0;
		uint64_t VertexShaderInvocations = 0;
		uint64_t ClippingInvocations = 0;
		uint64_t ClippingPrimitives = 0;
		uint64_t FragmentShaderInvocations = 0;
	};

	class VulkanCommandBuffer : public CommandBuffer
	{
	public:
		VulkanCommandBuffer(bool manualSubmission, uint32_t count);
		~VulkanCommandBuffer() override = default;

		void RT_Begin() override;
		void RT_End() override;
		void RT_Submit() const override;

		uint32_t RT_BeginTimestampQuery();
		void RT_EndTimestampQuery(uint32_t queryIndex) const;
		[[nodiscard]] float GetTimestamp(uint32_t frameIndex, uint32_t queryIndex = 0) const;
		[[nodiscard]] const PipelineStatistics& GetPipelineStatistics(const uint32_t frameIndex) const { return m_PipelineStatistics[frameIndex]; }

		void ResetCommandBuffer(uint32_t frameIndex) const;
		[[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const;

	private:
		VkCommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		std::vector<VkFence> m_Fences;

		std::vector<VkQueryPool> m_QueryPools;
		std::vector<std::vector<uint64_t>> m_Timestamps;
		std::vector<std::vector<float>> m_TimestampDeltas;
		uint32_t m_QueryIndex = 2;
		uint32_t m_QueryCount = 12;

		std::vector<VkQueryPool> m_PipelineQueryPools;
		std::vector<PipelineStatistics> m_PipelineStatistics;
		uint32_t m_PipelineQueryCount = 6;

		bool m_ManualSubmission;
	};
}
