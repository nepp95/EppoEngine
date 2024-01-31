#include "pch.h"
#include "Pipeline.h"

#include "Platform/Vulkan/VulkanPipeline.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<Pipeline> Pipeline::Create(const PipelineSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				EPPO_ASSERT(false);
				break;
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanPipeline>::Create(specification).As<Pipeline>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
