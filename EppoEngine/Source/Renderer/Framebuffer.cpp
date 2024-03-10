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
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanFramebuffer>::Create(specification).As<Framebuffer>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
