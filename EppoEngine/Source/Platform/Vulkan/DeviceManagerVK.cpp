#include "pch.h"
#include "Platform/Vulkan/DeviceManagerVK.h"

#include "Platform/Vulkan/Vulkan.h"

#include <GLFW/glfw3.h>

namespace Eppo
{
	DeviceManagerVK::DeviceManagerVK(const Ref<Window>& window, const DeviceParams& params)
		: DeviceManager(window, params)
	{
		CreateVulkanInstance();
		m_PhysicalDevice = CreateScopedPtr<PhysicalDevice>(m_Instance);
		m_LogicalDevice = CreateScopedPtr<LogicalDevice>(m_PhysicalDevice);
		CreateNvrhiDevice();
	}

	auto DeviceManagerVK::Init() -> void
	{
		VkSurfaceKHR surface = nullptr;
		VK_CHECK(glfwCreateWindowSurface(m_Instance, m_Window->GetNative(), nullptr, &surface), "Failed to create window surface!");
		EP_ASSERT(surface);

		m_Swapchain = CreateScopedPtr<Swapchain>(surface);
		m_Swapchain->CreateSwapchain();
	}

	auto DeviceManagerVK::Shutdown() -> void
	{
		m_Renderer = nullptr;
		m_Swapchain = nullptr;

		m_Device->runGarbageCollection();
		m_Device = nullptr;
		m_ValidationLayer = nullptr;

		m_LogicalDevice = nullptr;
		m_PhysicalDevice = nullptr;

		if (g_EnableValidationLayers)
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

		vkDestroyInstance(m_Instance, nullptr);
	}

	auto DeviceManagerVK::GetDevice() const -> nvrhi::IDevice*
	{
		if (m_ValidationLayer)
			return m_ValidationLayer;

		return m_Device;
	}

	auto DeviceManagerVK::BeginFrame() -> bool
	{
		return m_Swapchain->BeginFrame();
	}

	auto DeviceManagerVK::Present() -> bool
	{
		return m_Swapchain->Present();
	}

	auto DeviceManagerVK::CreateVulkanInstance() -> void
	{
		// Create instance
		const VkApplicationInfo appInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = "EppoEngine",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "EppoEngine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_3,
		};

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		#if !defined(EP_DIST)
		m_Params.RequiredVulkanInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		#endif

		for (const auto& extension : extensions)
			m_Params.RequiredVulkanInstanceExtensions.emplace_back(extension);

		for (const auto& extension : m_Params.RequiredVulkanInstanceExtensions)
			Log::Info("Enabled instance extension '{}'", extension);

		VkInstanceCreateInfo instanceInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = static_cast<uint32_t>(m_Params.RequiredVulkanInstanceExtensions.size()),
			.ppEnabledExtensionNames = m_Params.RequiredVulkanInstanceExtensions.data(),
		};

		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
		if (g_EnableValidationLayers)
		{
			instanceInfo.enabledLayerCount = static_cast<uint32_t>(g_ValidationLayers.size());
			instanceInfo.ppEnabledLayerNames = g_ValidationLayers.data();

			debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugMessengerInfo.pNext = nullptr;
			debugMessengerInfo.flags = 0;
			debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugMessengerInfo.pfnUserCallback = VulkanDebugCallback;
			debugMessengerInfo.pUserData = nullptr;

			instanceInfo.pNext = &debugMessengerInfo;
		}

		VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_Instance), "Failed to create vulkan instance!");
		EP_ASSERT(m_Instance);

		if (g_EnableValidationLayers)
		{
			VK_CHECK(CreateDebugUtilsMessengerEXT(m_Instance, &debugMessengerInfo, nullptr, &m_DebugMessenger), "Failed to create vulkan debug messenger!");
			EP_ASSERT(m_DebugMessenger);
		}
	}

	auto DeviceManagerVK::CreateNvrhiDevice() -> void
	{
		const auto& indices = m_PhysicalDevice->GetQueueFamilyIndices();

		std::vector<const char*> deviceExtensions(g_DeviceExtensions.begin(), g_DeviceExtensions.end());

		nvrhi::vulkan::DeviceDesc deviceDesc{
			.errorCB = &m_MessageCallback,
			.instance = m_Instance,
			.physicalDevice = m_PhysicalDevice->GetNative(),
			.device = m_LogicalDevice->GetNative(),
			.graphicsQueue = m_LogicalDevice->GetGraphicsQueue(),
			.graphicsQueueIndex = indices.Graphics,
			.transferQueue = m_LogicalDevice->GetTransferQueue(),
			.transferQueueIndex = indices.Transfer,
			.computeQueue = m_LogicalDevice->GetComputeQueue(),
			.computeQueueIndex = indices.Compute,
			.instanceExtensions = m_Params.RequiredVulkanInstanceExtensions.data(),
			.numInstanceExtensions = m_Params.RequiredVulkanInstanceExtensions.size(),
			.deviceExtensions = deviceExtensions.data(),
			.numDeviceExtensions = deviceExtensions.size(),
		};

		m_Device = nvrhi::vulkan::createDevice(deviceDesc);

		if (g_EnableValidationLayers)
			m_ValidationLayer = nvrhi::validation::createValidationLayer(m_Device);
	}
}