#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	struct PipelineSpecification
	{
		Ref<Shader> Shader;
	};

	class Pipeline
	{
	public:
		Pipeline(const PipelineSpecification& specification);
		~Pipeline();

		VkPipeline GetPipeline() const { return m_Pipeline; }

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
	};
}
