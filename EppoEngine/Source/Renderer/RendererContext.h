#pragma once

#include "Debug/Profiler.h"
#include "Renderer/LogicalDevice.h"
#include "Renderer/Swapchain.h"

#include <deque>

struct GLFWwindow;

namespace Eppo
{
	class RendererContext
	{
	public:
		RendererContext(GLFWwindow* windowHandle);
		~RendererContext() = default;

		void Init();
		void Shutdown();

		void WaitIdle();

		Ref<PhysicalDevice> GetPhysicalDevice() { return m_PhysicalDevice; }
		Ref<LogicalDevice> GetLogicalDevice() { return m_LogicalDevice; }
		Ref<Swapchain> GetSwapchain() { return m_Swapchain; }
		GLFWwindow* GetWindowHandle() { return m_WindowHandle; }

		TracyVkCtx GetCurrentProfilerContext() { return m_TracyContexts[m_Swapchain->GetCurrentImageIndex()]; }

		void SubmitResourceFree(std::function<void()> fn);

		static Ref<RendererContext> Get();
		VkInstance& GetVulkanInstance() { return m_VulkanInstance; }

	private:
		bool HasValidationSupport();
		std::vector<const char*> GetRequiredExtensions();

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
		std::vector<TracyVkCtx> m_TracyContexts;

		static RendererContext* s_Instance;
	};
}
