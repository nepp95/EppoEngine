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

		void ResetCommandBuffer(uint32_t frameIndex);
		VkCommandBuffer GetCurrentCommandBuffer();

	private:
		VkCommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		std::vector<VkFence> m_Fences;

		bool m_ManualSubmission;
	};
}
