#include "pch.h"
#include "Image.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Image> Image::Create(const ImageSpecification& specification)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPIType::OpenGL:
			{
				EPPO_ASSERT(false);
				break;
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanImage>::Create(specification);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
