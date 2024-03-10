#pragma once

#include "Renderer/Pipeline.h"
#include "Renderer/Buffer/IndexBuffer.h"
#include "Renderer/Buffer/VertexBuffer.h"

namespace Eppo
{
	class OpenGLPipeline : public Pipeline
	{
	public:
		OpenGLPipeline(const PipelineSpecification& specification);
		virtual ~OpenGLPipeline();

		void AddVertexBuffer(Ref<VertexBuffer> vertexBuffer);

		void Bind() const;
		void UpdateUniforms(Ref<UniformBufferSet> uniformBufferSet) override;

	private:
		uint32_t m_RendererID;
		uint32_t m_bufferIndex = 0;

		std::vector<Ref<VertexBuffer>> m_VertexBuffers;
	};
}
