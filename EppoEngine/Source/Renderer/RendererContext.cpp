#include "pch.h"
#include "RendererContext.h"

#include "Core/Application.h"
#include "Renderer/Allocator.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	RendererContext::RendererContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		EPPO_PROFILE_FUNCTION("RendererContext::RendererContext");
		EPPO_ASSERT(windowHandle);
	}

	void RendererContext::Init()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Init");

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "EppoEngine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "EppoEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		auto extensions = GetRequiredExtensions();

		VkInstanceCreateInfo instanceInfo{};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo = &appInfo;
		instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instanceInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
		if (VulkanConfig::EnableValidation)
		{
			instanceInfo.enabledLayerCount = static_cast<uint32_t>(VulkanConfig::ValidationLayers.size());
			instanceInfo.ppEnabledLayerNames = VulkanConfig::ValidationLayers.data();

			debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugInfo.pfnUserCallback = DebugCallback;
			debugInfo.pUserData = nullptr;

			instanceInfo.pNext = &debugInfo;
		} else
		{
			instanceInfo.enabledLayerCount = 0;
			instanceInfo.pNext = nullptr;
		}

		VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &s_Instance), "Failed to create instance!");

		// Debug messenger
		if (VulkanConfig::EnableValidation)
			VK_CHECK(CreateDebugUtilsMessengerEXT(s_Instance, &debugInfo, nullptr, &m_DebugMessenger), "Failed to create debug messenger!");

		// Devices
		m_PhysicalDevice = CreateRef<PhysicalDevice>();
		m_LogicalDevice = CreateRef<LogicalDevice>(m_PhysicalDevice);

		// Allocator
		Allocator::Init();

		// Swapchain
		m_Swapchain = CreateRef<Swapchain>(m_LogicalDevice);
	}

	void RendererContext::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Shutdown");

		for (uint32_t i = 0; i < m_ResourceFreeCommandCount; i++)
		{
			m_ResourceFreeCommands.back()();
			m_ResourceFreeCommands.pop_back();
		}

		if (VulkanConfig::EnableValidation)
			DestroyDebugUtilsMessengerEXT(s_Instance, m_DebugMessenger, nullptr);

		vkDestroyInstance(s_Instance, nullptr);
	}

	void RendererContext::WaitIdle()
	{
		vkDeviceWaitIdle(m_LogicalDevice->GetNativeDevice());
	}

	void RendererContext::SubmitResourceFree(std::function<void()> fn)
	{
		m_ResourceFreeCommands.emplace_back(fn);
		m_ResourceFreeCommandCount++;
	}

	Ref<RendererContext> RendererContext::Get()
	{
		EPPO_PROFILE_FUNCTION("RendererContext::Get");

		return Application::Get().GetWindow().GetRendererContext();
	}

	std::vector<const char*> RendererContext::GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (VulkanConfig::EnableValidation)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}
}
