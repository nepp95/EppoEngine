#include "pch.h"
#include "Renderer/Image.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Image> Image::Create(const ImageSpecification& specification)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanImage>(specification);
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
