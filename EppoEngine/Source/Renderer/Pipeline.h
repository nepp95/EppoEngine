#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexBufferLayout.h"
#include "Platform/Vulkan/Vulkan.h"

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

	class Pipeline : public RefCounter
	{
	public:
		virtual ~Pipeline() {};

		const PipelineSpecification& GetSpecification() const {	return m_Specification; }
		
		static Ref<Pipeline> Create(const PipelineSpecification& specification);

	protected:
		Pipeline(const PipelineSpecification& specification);

	protected:
		PipelineSpecification m_Specification;
	};
}
