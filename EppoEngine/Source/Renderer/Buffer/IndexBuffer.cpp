#include "pch.h"
#include "IndexBuffer.h"

#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<IndexBuffer> IndexBuffer::Create(void* data, uint32_t size)
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
				return Ref<VulkanIndexBuffer>::Create(data, size);
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
