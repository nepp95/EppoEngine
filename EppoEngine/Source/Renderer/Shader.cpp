#include "pch.h"
#include "Shader.h"

#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Shader> Shader::Create(const ShaderSpecification& specification)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanShader>(specification);
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
