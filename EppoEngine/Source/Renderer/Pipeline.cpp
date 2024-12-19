#include "pch.h"
#include "Pipeline.h"

#include "Platform/Vulkan/VulkanPipeline.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Pipeline> Pipeline::Create(const PipelineSpecification& specification)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanPipeline>(specification);
		}

		EPPO_ASSERT(false)
		return nullptr;
	}
}
