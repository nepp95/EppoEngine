#include "pch.h"
#include "SceneRenderer.h"

#include "Platform/Vulkan/VulkanSceneRenderer.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	Ref<SceneRenderer> SceneRenderer::Create(Ref<Scene> scene, const RenderSpecification& renderSpec)
	{
		switch (RendererContext::GetAPI())
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanSceneRenderer>(scene, renderSpec);
		}

		EPPO_ASSERT(false)
		return nullptr;
	}
}
