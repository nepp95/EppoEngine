#include "pch.h"
#include "VulkanContext.h"

#include "Platform/Vulkan/VulkanAllocator.h"

#include <GLFW/glfw3.h>
#include <volk.h>

namespace Eppo
{
	VulkanContext::VulkanContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		EPPO_ASSERT(windowHandle);
	}

	void VulkanContext::Init()
	{
		// Initialize Volk for loading Vulkan functions
		VK_CHECK(volkInitialize(), "Failed to initialize volk!");

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
		}
		else
		{
			instanceInfo.enabledLayerCount = 0;
			instanceInfo.pNext = nullptr;
		}

		VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &s_Instance), "Failed to create instance!");

		// After creating vulkan instance, load all required vulkan entrypoints
		volkLoadInstance(s_Instance);

		// Debug messenger
		if (VulkanConfig::EnableValidation)
			VK_CHECK(CreateDebugUtilsMessengerEXT(s_Instance, &debugInfo, nullptr, &m_DebugMessenger), "Failed to create debug messenger!");

		// Devices
		m_PhysicalDevice = CreateRef<VulkanPhysicalDevice>();
		m_LogicalDevice = CreateRef<VulkanLogicalDevice>(m_PhysicalDevice);
	
		// Allocator
		VulkanAllocator::Init();

		// Swapchain
		m_Swapchain = CreateRef<VulkanSwapchain>(m_LogicalDevice);

		// Create tracy profiler context
		VkCommandBuffer cmd = m_LogicalDevice->GetCommandBuffer(false);

		#if defined(TRACY_ENABLE)
		m_TracyContext = TracyVkContext(
			m_PhysicalDevice->GetNativeDevice(),
			m_LogicalDevice->GetNativeDevice(),
			m_LogicalDevice->GetGraphicsQueue(),
			cmd
		)
		#endif

		m_LogicalDevice->FreeCommandBuffer(cmd);
	}

	void VulkanContext::Shutdown()
	{
		#if defined(TRACY_ENABLE)
		EPPO_MEM_WARN("Releasing tracy context {}", (void*)m_TracyContext);
		TracyVkDestroy(m_TracyContext)
		#endif

		m_GarbageCollector.Shutdown();

		if (VulkanConfig::EnableValidation)
			DestroyDebugUtilsMessengerEXT(s_Instance, m_DebugMessenger, nullptr);

		EPPO_MEM_WARN("Releasing vulkan instance {}", (void*)s_Instance);
		vkDestroyInstance(s_Instance, nullptr);
	}

	void VulkanContext::BeginFrame()
	{
		m_Swapchain->BeginFrame();
	}

	void VulkanContext::PresentFrame()
	{
		m_Swapchain->PresentFrame();
	}

	void VulkanContext::WaitIdle()
	{
		vkDeviceWaitIdle(m_LogicalDevice->GetNativeDevice());
	}

	void VulkanContext::SubmitResourceFree(std::function<void()> fn, bool freeOnShutdown)
	{
		m_GarbageCollector.SubmitFreeFn(fn, freeOnShutdown);
	}

	void VulkanContext::RunGC(uint32_t frameNumber)
	{
		m_GarbageCollector.Update(frameNumber);
	}

	Ref<VulkanContext> VulkanContext::Get()
	{
		return std::static_pointer_cast<VulkanContext>(RendererContext::Get());
	}

	std::vector<const char*> VulkanContext::GetRequiredExtensions() const
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (VulkanConfig::EnableValidation)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		return extensions;
	}
}
