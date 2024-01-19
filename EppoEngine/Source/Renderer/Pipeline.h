#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexBufferLayout.h"

struct VkPushConstantRange;

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

		virtual const PipelineSpecification& GetSpecification() const = 0;
		
		static Ref<Pipeline> Create(const PipelineSpecification& specification);
	};
}
