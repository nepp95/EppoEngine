#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/RendererContext.h"

struct GLFWwindow;

namespace Eppo
{
	class VulkanContext : public RendererContext
	{
	public:
		VulkanContext(GLFWwindow* windowHandle);
		virtual ~VulkanContext() = default;

		virtual void Init() override;
		virtual void Shutdown() override;

	private:
		GLFWwindow* m_WindowHandle;

		VkInstance m_VulkanInstance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;


	};
}
