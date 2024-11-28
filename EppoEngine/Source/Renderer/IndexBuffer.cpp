#include "pch.h"
#include "IndexBuffer.h"

#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<IndexBuffer> IndexBuffer::Create(uint32_t size)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanIndexBuffer>(size);
		}

		EPPO_ASSERT(false);
		return nullptr;
	}

	Ref<IndexBuffer> IndexBuffer::Create(void* data, uint32_t size)
	{
		Buffer buffer = Buffer::Copy(data, size);

		return Create(buffer);
	}

	Ref<IndexBuffer> IndexBuffer::Create(Buffer buffer)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanIndexBuffer>(buffer);
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
