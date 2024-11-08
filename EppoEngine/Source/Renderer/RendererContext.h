#pragma once

#include "Debug/Profiler.h"
#include "Renderer/LogicalDevice.h"
#include "Renderer/PhysicalDevice.h"
#include "Renderer/Swapchain.h"
#include "Renderer/Vulkan.h"

#include <deque>
#include <functional>

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

		void SubmitResourceFree(std::function<void()> fn);

		Ref<LogicalDevice> GetLogicalDevice() const { return m_LogicalDevice; }
		Ref<PhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		Ref<Swapchain> GetSwapchain() const { return m_Swapchain; }

		static VkInstance GetVulkanInstance() { return s_Instance; }
		GLFWwindow* GetWindowHandle() { return m_WindowHandle; }

		static Ref<RendererContext> Get();

	private:
		std::vector<const char*> GetRequiredExtensions();

	private:
		GLFWwindow* m_WindowHandle = nullptr;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		Ref<PhysicalDevice> m_PhysicalDevice;
		Ref<LogicalDevice> m_LogicalDevice;
		Ref<Swapchain> m_Swapchain;

		std::deque<std::function<void()>> m_ResourceFreeCommands;
		uint32_t m_ResourceFreeCommandCount = 0;

		inline static VkInstance s_Instance;
	};
}
