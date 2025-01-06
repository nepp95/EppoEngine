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
			case RendererAPI::Vulkan:	return CreateRef<VulkanRenderer>();
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
