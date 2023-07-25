#include "pch.h"
#include "RendererContext.h"

#include "Core/Application.h"

#include <glfw/glfw3.h>

namespace Eppo
{
	RendererContext* RendererContext::s_Instance = nullptr;

	RendererContext::RendererContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		EPPO_ASSERT(!s_Instance);
		s_Instance = this;
		EPPO_ASSERT(windowHandle);
	}

	void RendererContext::Init()
	{
		// Vulkan instance
		if (VulkanConfig::EnableValidation)
			EPPO_ASSERT(HasValidationSupport());

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "EppoEngine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "EppoEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		auto extensions = GetRequiredExtensions();

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (VulkanConfig::EnableValidation)
		{
			debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugCreateInfo.pfnUserCallback = DebugCallback;
			debugCreateInfo.pUserData = nullptr;

			createInfo.enabledLayerCount = (uint32_t)VulkanConfig::ValidationLayers.size();
			createInfo.ppEnabledLayerNames = VulkanConfig::ValidationLayers.data();
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_VulkanInstance), "Failed to create instance!");

		// Debug messenger
		if (VulkanConfig::EnableValidation)
			VK_CHECK(CreateDebugUtilsMessengerEXT(m_VulkanInstance, &debugCreateInfo, nullptr, &m_DebugMessenger), "Failed to create debug messenger!");

		// Devices
		m_PhysicalDevice = CreateRef<PhysicalDevice>();
		m_LogicalDevice = CreateRef<LogicalDevice>(m_PhysicalDevice);
	}

	void RendererContext::Shutdown()
	{
		for (uint32_t i = 0; i < m_ResourceFreeCommandCount; i++)
		{
			m_ResourceFreeCommands.back()();
			m_ResourceFreeCommands.pop_back();
		}

		if (VulkanConfig::EnableValidation)
			DestroyDebugUtilsMessengerEXT(m_VulkanInstance, m_DebugMessenger, nullptr);
	
		vkDestroyInstance(m_VulkanInstance, nullptr);
	}

	void RendererContext::SubmitResourceFree(std::function<void()> fn)
	{
		m_ResourceFreeCommands.push_back(fn);
		m_ResourceFreeCommandCount++;
	}

	Ref<RendererContext> RendererContext::Get()
	{
		return Application::Get().GetWindow().GetRendererContext();
	}

	bool RendererContext::HasValidationSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : VulkanConfig::ValidationLayers)
		{
			bool layerFound{ false };

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}

		return true;
	}

	std::vector<const char*> RendererContext::GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount{ 0 };
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (VulkanConfig::EnableValidation)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}
}
