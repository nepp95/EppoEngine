#include "pch.h"
#include "Framebuffer.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& specification)
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
				return Ref<VulkanFramebuffer>::Create(specification).As<Framebuffer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		/*m_Specification.Width = width;
		m_Specification.Height = height;

		Cleanup();
		Create();*/
	}
}
