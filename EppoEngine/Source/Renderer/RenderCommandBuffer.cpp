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
			case RendererAPIType::OpenGL:
			{
				EPPO_ASSERT(false);
				break;
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanRenderCommandBuffer>::Create(count).As<RenderCommandBuffer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
