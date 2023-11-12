#pragma once

#include "Renderer/Vulkan.h"

namespace Eppo
{
	class RenderCommandBuffer
	{
	public:
		RenderCommandBuffer(uint32_t count = 0);
		~RenderCommandBuffer();

		void Begin();
		void End();
		void Submit();

		uint32_t BeginTimestampQuery();
		void EndTimestampQuery(uint32_t queryIndex);

		float GetTimestamp(uint32_t imageIndex, uint32_t queryIndex) const;

		VkCommandBuffer GetCurrentCommandBuffer() const;

	private:
		VkCommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		
		std::vector<VkQueryPool> m_QueryPools;
		std::vector<std::vector<uint64_t>> m_Timestamps;
		uint32_t m_QueryIndex = 2;
		uint32_t m_QueryCount = 6;

		std::vector<VkFence> m_Fences;
	};
}
