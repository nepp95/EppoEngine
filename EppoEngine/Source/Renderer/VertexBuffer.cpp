#include "pch.h"
#include "VertexBuffer.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<VertexBuffer> VertexBuffer::Create(void* data, uint32_t size)
	{
		Buffer buffer = Buffer::Copy(data, size);

		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:
			{
				Ref<VertexBuffer> vb = CreateRef<VulkanVertexBuffer>(buffer);
				buffer.Release();
				return vb;
			}
		}

		EPPO_ASSERT(false);
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(Buffer buffer)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanVertexBuffer>(buffer);
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
