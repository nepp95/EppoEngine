#pragma once

#include "Renderer/RenderCommandBuffer.h"

namespace Eppo
{
	class OpenGLRenderCommandBuffer : public RenderCommandBuffer
	{
	public:
		OpenGLRenderCommandBuffer(uint32_t count);
		~OpenGLRenderCommandBuffer();

		void Begin() override;
		void End() override;
		void Submit() override;

		uint32_t BeginTimestampQuery() override;
		void EndTimestampQuery(uint32_t queryIndex) override;

		float GetTimestamp(uint32_t imageIndex, uint32_t queryIndex) const override;
		const PipelineStatistics& GetPipelineStatistics(uint32_t imageIndex) const override;

	private:
		uint32_t m_QueryIndex = 1;

		std::vector<std::vector<uint64_t>> m_Timestamps;

		std::vector<PipelineStatistics> m_PipelineStatistics;
	};
}
