#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<RenderCommandBuffer> RenderCommandBuffer::Create(uint32_t count)
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
				return Ref<VulkanRenderCommandBuffer>::Create(count).As<RenderCommandBuffer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
