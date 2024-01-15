#include "pch.h"
#include "VertexBuffer.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<VertexBuffer> VertexBuffer::Create(void* data, uint32_t size)
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
				return CreateRef<VulkanVertexBuffer>(data, size);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
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
				return CreateRef<VulkanVertexBuffer>(size);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
