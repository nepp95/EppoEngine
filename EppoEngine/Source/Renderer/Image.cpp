#include "pch.h"
#include "Image.h"

#include "Platform/OpenGL/OpenGLImage.h"
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
				return Ref<OpenGLImage>::Create(specification).As<Image>();
				break;
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanImage>::Create(specification).As<Image>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
