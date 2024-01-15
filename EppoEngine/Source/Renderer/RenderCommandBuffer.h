#pragma once

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

	class RenderCommandBuffer
	{
	public:
		virtual ~RenderCommandBuffer() {};

		virtual void Begin() = 0;
		virtual void End() = 0;
		virtual void Submit() = 0;

		virtual uint32_t BeginTimestampQuery() = 0;
		virtual void EndTimestampQuery(uint32_t queryIndex) = 0;

		virtual float GetTimestamp(uint32_t imageIndex, uint32_t queryIndex = 0) const = 0;
		virtual const PipelineStatistics& GetPipelineStatistics(uint32_t imageIndex) const = 0;

		static Ref<RenderCommandBuffer> Create(uint32_t count);
	};
}
