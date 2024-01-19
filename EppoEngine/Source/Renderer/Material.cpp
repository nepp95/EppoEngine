#include "pch.h"
#include "Material.h"

#include "Platform/Vulkan/VulkanMaterial.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<Material> Material::Create(Ref<Shader> shader)
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
				return Ref<VulkanMaterial>::Create(shader).As<Material>();
			}

			EPPO_ASSERT(false);
			return nullptr;
		}
	}
}
