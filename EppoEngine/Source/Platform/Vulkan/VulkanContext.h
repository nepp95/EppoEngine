#pragma once

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

		virtual void Init() override;
		virtual void Shutdown() override;

		void SubmitResourceFree(std::function<void()> fn);

		const Ref<PhysicalDevice>& GetPhysicalDevice() const { return m_PhysicalDevice; }
		const Ref<LogicalDevice>& GetLogicalDevice() const { return m_LogicalDevice; }
		const Ref<Swapchain>& GetSwapchain() const { return m_Swapchain; }

		static Ref<VulkanContext> Get();

	private:
		bool HasValidationSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;

	private:
		GLFWwindow* m_WindowHandle;

		VkInstance m_VulkanInstance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		Ref<PhysicalDevice> m_PhysicalDevice;
		Ref<LogicalDevice> m_LogicalDevice;
		Ref<Swapchain> m_Swapchain;

		// Vulkan resource management
		std::deque<std::function<void()>> m_ResourceFreeCommands;
		uint32_t m_ResourceFreeCommandCount = 0;
	};
}
