#include "pch.h"
#include "Shader.h"

#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Shader> Create(const ShaderSpecification& specification)
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
				return Ref<VulkanShader>::Create(specification).As<Shader>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
