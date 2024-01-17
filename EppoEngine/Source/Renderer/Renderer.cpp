#include "pch.h"
#include "Renderer.h"

#include "Platform/Vulkan/VulkanRenderer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Renderer> Renderer::Create()
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
				return Ref<VulkanRenderer>::Create().As<Renderer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
