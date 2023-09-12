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

		// TODO: Get from shader
		std::vector<VkPushConstantRange> PushConstants;

		bool DepthTesting = false;
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
