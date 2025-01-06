#include "pch.h"
#include "Renderer/DebugRenderer.h"

#include "Platform/Vulkan/VulkanDebugRenderer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<DebugRenderer> DebugRenderer::Create()
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanDebugRenderer>();
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
