#include "pch.h"
#include "VulkanContext.h"

#include "Core/Application.h"
#include "Platform/Vulkan/VulkanAllocator.h"

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
		m_PhysicalDevice = Ref<VulkanPhysicalDevice>::Create();
		m_LogicalDevice = Ref<VulkanLogicalDevice>::Create(m_PhysicalDevice);

		// Allocator
		VulkanAllocator::Init();

		// Swapchain
		m_Swapchain = Ref<VulkanSwapchain>::Create();
	}

	void VulkanContext::Shutdown()
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::Shutdown");

		for (uint32_t i = 0; i < m_ResourceFreeCommandCount; i++)
		{
			m_ResourceFreeCommands.back()();
			m_ResourceFreeCommands.pop_back();
		}

		if (VulkanConfig::EnableValidation)
			DestroyDebugUtilsMessengerEXT(m_VulkanInstance, m_DebugMessenger, nullptr);

		vkDestroyInstance(m_VulkanInstance, nullptr);
	}

	void VulkanContext::WaitIdle()
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::WaitIdle");

		VkDevice device = m_LogicalDevice->GetNativeDevice();
		vkDeviceWaitIdle(device);
	}

	void VulkanContext::SubmitResourceFree(std::function<void()> fn)
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::SubmitResourceFree");

		m_ResourceFreeCommands.push_back(fn);
		m_ResourceFreeCommandCount++;
	}

	bool VulkanContext::HasValidationSupport() const
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::HasValidationSupport");

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

	std::vector<const char*> VulkanContext::GetRequiredExtensions() const
	{
		EPPO_PROFILE_FUNCTION("VulkanContext::GetRequiredExtensions");

		uint32_t glfwExtensionCount{ 0 };
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (VulkanConfig::EnableValidation)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}
}
