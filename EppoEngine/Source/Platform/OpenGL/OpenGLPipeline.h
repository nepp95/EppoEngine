#pragma once

#include "Renderer/Pipeline.h"

namespace Eppo
{
	class OpenGLPipeline : public Pipeline
	{
	public:
		OpenGLPipeline(const PipelineSpecification& specification);
		virtual ~OpenGLPipeline();

	private:
	};
}
