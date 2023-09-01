#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexBufferLayout.h"

namespace Eppo
{
	struct PipelineSpecification
	{
		Ref<Framebuffer> Framebuffer;
		Ref<Shader> Shader;
		VertexBufferLayout Layout;
	};

	class Pipeline
	{
	public:
		Pipeline(const PipelineSpecification& specification);
		~Pipeline();

		const PipelineSpecification& GetSpecification() const { return m_Specification; }

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
	};
}
