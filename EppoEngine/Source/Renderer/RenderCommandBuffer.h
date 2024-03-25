#pragma once

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

	private:
		uint32_t m_QueryRendererID;
		uint64_t m_Timestamp;
	};
}
