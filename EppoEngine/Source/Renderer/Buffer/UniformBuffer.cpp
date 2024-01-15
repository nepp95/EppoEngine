#include "pch.h"
#include "UniformBuffer.h"

#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
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
				return Ref<VulkanUniformBuffer>::Create(size, binding);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
