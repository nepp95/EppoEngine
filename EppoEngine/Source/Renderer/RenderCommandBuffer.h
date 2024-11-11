#pragma once

#include "Renderer/Vulkan.h"

namespace Eppo
{
	class RenderCommandBuffer
	{
	public:
		RenderCommandBuffer(bool manualSubmission = true, uint32_t count = 0);
		~RenderCommandBuffer() = default;

		void RT_Begin();
		void RT_End();
		void RT_Submit();

		uint32_t BeginTimestampQuery();
		void EndTimestampQuery(uint32_t queryIndex);
		float GetTimestamp(uint32_t frameIndex, uint32_t queryIndex = 0) const;

		void ResetCommandBuffer(uint32_t frameIndex);
		VkCommandBuffer GetCurrentCommandBuffer();

	private:
		VkCommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		std::vector<VkFence> m_Fences;

		std::vector<VkQueryPool> m_QueryPools;
		std::vector<std::vector<uint64_t>> m_Timestamps;
		std::vector<std::vector<float>> m_TimestampDeltas;
		uint32_t m_QueryIndex = 2;
		uint32_t m_QueryCount = 8;

		bool m_ManualSubmission;
	};
}
