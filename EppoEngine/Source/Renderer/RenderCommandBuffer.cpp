#include "pch.h"
#include "RenderCommandBuffer.h"

#include "Platform/OpenGL/OpenGLRenderCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<RenderCommandBuffer> RenderCommandBuffer::Create(uint32_t count)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLRenderCommandBuffer>::Create(count).As<RenderCommandBuffer>();
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
