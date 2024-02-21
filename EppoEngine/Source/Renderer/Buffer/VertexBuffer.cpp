#include "pch.h"
#include "VertexBuffer.h"

#include "Platform/OpenGL/OpenGLVertexBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<VertexBuffer> VertexBuffer::Create(void* data, uint32_t size)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLVertexBuffer>::Create(data, size).As<VertexBuffer>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanVertexBuffer>::Create(data, size).As<VertexBuffer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLVertexBuffer>::Create(size).As<VertexBuffer>();
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanVertexBuffer>::Create(size).As<VertexBuffer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
