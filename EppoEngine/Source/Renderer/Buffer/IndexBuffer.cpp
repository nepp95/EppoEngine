#include "pch.h"
#include "IndexBuffer.h"

#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<IndexBuffer> IndexBuffer::Create(void* data, uint32_t size)
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
				return CreateRef<VulkanIndexBuffer>(data, size);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
