#include "pch.h"
#include "Shader.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Renderer/RendererAPI.h"

namespace Eppo
{
	Ref<Shader> Shader::Create(const ShaderSpecification& specification)
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::OpenGL:
			{
				return Ref<OpenGLShader>::Create(specification).As<Shader>();
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
