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
			case RendererAPIType::OpenGL:
			{
				EPPO_ASSERT(false);
				break;
			}

			case RendererAPIType::Vulkan:
			{
				return Ref<VulkanShader>::Create(specification).As<Shader>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
