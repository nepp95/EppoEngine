#include "pch.h"

#include "VulkanContext.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	VulkanContext::VulkanContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::VulkanContext");

		EPPO_ASSERT(m_WindowHandle);
	}

	void VulkanContext::Init()
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::Init");

		// Vulkan instance
		if (VulkanConfig::EnableValidation)
		{
			EPPO_ASSERT(HasValidationSupport());
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "EppoEngine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "EppoEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;


	}

	void VulkanContext::Shutdown()
	{

	}
}
