#pragma once

#include "Debug/Profiler.h"
#include "Platform/Vulkan/LogicalDevice.h"
#include "Platform/Vulkan/PhysicalDevice.h"
#include "Platform/Vulkan/Swapchain.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/RendererContext.h"

#include <deque>

struct GLFWwindow;

namespace Eppo
{
	class VulkanContext : public RendererContext
	{
	public:
		VulkanContext(GLFWwindow* windowHandle);
		virtual ~VulkanContext() = default;

		void Init() override;
		void Shutdown() override;

		void WaitIdle();

		void SubmitResourceFree(std::function<void()> fn);

		GLFWwindow* GetWindowHandle() { return m_WindowHandle; }

		VkInstance GetVulkanInstance() const { return m_VulkanInstance; }
		const Ref<PhysicalDevice>& GetPhysicalDevice() const { return m_PhysicalDevice; }
		const Ref<LogicalDevice>& GetLogicalDevice() const { return m_LogicalDevice; }
		const Ref<Swapchain>& GetSwapchain() const { return m_Swapchain; }

		//TracyVkCtx GetCurrentProfilerContext() { return m_TracyContexts[m_Swapchain->GetCurrentImageIndex()]; }

	private:
		bool HasValidationSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;

	private:
		GLFWwindow* m_WindowHandle = nullptr;

		VkInstance m_VulkanInstance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		Ref<PhysicalDevice> m_PhysicalDevice;
		Ref<LogicalDevice> m_LogicalDevice;
		Ref<Swapchain> m_Swapchain;

		// Vulkan resource management
		std::deque<std::function<void()>> m_ResourceFreeCommands;
		uint32_t m_ResourceFreeCommandCount = 0;

		// Tracy profiler context
		//std::vector<TracyVkCtx> m_TracyContexts;
	};
}
