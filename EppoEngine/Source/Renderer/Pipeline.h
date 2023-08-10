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

		const PipelineSpecification& GetSpecification() const { return m_Specification; }

		VkPipeline GetPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
	};
}
