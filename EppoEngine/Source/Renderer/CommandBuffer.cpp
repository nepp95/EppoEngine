#include "pch.h"
#include "CommandBuffer.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<CommandBuffer> CommandBuffer::Create(bool manualSubmission, uint32_t count)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanCommandBuffer>(manualSubmission, count);
		}

		EPPO_ASSERT(false)
		return nullptr;
	}
}
