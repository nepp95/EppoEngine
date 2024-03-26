#pragma once

namespace Eppo
{
	class RenderCommandBuffer
	{
	public:
		RenderCommandBuffer(uint32_t count = 0);
		~RenderCommandBuffer();

		void RT_Begin();
		void RT_End();
		void RT_Submit();

	private:
		uint32_t m_QueryRendererID;
		uint64_t m_Timestamp;
	};
}
