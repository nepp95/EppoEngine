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

		VkCommandBuffer GetCurrentCommandBuffer() const;

	private:
		VkCommandPool m_CommandPool;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		
		std::vector<VkFence> m_Fences;
	};
}
