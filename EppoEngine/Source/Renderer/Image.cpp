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
			case RendererAPI::OpenGL:
			{
				EPPO_ASSERT(false);
				break;
			}

			case RendererAPI::Vulkan:
			{
				return Ref<VulkanImage>::Create(specification);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
