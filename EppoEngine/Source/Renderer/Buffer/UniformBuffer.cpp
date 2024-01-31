#include "pch.h"
#include "UniformBuffer.h"

#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
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
				return Ref<VulkanUniformBuffer>::Create(size, binding);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
