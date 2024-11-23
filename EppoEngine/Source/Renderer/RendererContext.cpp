#include "pch.h"
#include "RendererContext.h"

#include "Core/Application.h"
#include "Platform/Vulkan/VulkanContext.h"

#include <GLFW/glfw3.h>
#include <tracy/TracyOpenGL.hpp>

namespace Eppo
{
	RendererAPI RendererContext::s_API = RendererAPI::Vulkan;

	Ref<RendererContext> RendererContext::Get()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Get");

		return Application::Get().GetWindow().GetRendererContext();
	}

	Ref<RendererContext> RendererContext::Create(GLFWwindow* windowHandle)
	{
		switch (s_API)
		{
			case RendererAPI::Vulkan:	return CreateRef<VulkanContext>(windowHandle);
		}

		EPPO_ASSERT(false);
		return nullptr;
	}
}
