#pragma once

#include "Renderer/LogicalDevice.h"

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

		std::deque<std::function<void()>> m_ResourceFreeCommands;
		uint32_t m_ResourceFreeCommandCount = 0;

		static RendererContext* s_Instance;
	};
}
