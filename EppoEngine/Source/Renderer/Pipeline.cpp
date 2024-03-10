#include "pch.h"
#include "Pipeline.h"

#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Pipeline::Pipeline(const PipelineSpecification& specification)
		: m_Specification(specification)
	{}

	Ref<Pipeline> Pipeline::Create(const PipelineSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLPipeline>::Create(specification).As<Pipeline>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanPipeline>::Create(specification).As<Pipeline>();
			}
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
