#include "pch.h"
#include "Image.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<Image> Image::Create(const ImageSpecification& specification)
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
				return Ref<VulkanImage>::Create(specification);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
