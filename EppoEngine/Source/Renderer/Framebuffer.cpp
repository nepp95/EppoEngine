#include "pch.h"
#include "Framebuffer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLFramebuffer>::Create(specification).As<Framebuffer>();
				break;
			}

			case RendererAPIType::Vulkan:
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
