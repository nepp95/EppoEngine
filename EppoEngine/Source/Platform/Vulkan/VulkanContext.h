#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Platform/Vulkan/VulkanLogicalDevice.h"
#include "Platform/Vulkan/VulkanPhysicalDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Renderer/GarbageCollector.h"
#include "Renderer/RendererContext.h"

namespace Eppo
{
	class VulkanContext : public RendererContext
	{
	public:
		VulkanContext(GLFWwindow* windowHandle);
		virtual ~VulkanContext() = default;

		void Init() override;
		void Shutdown() override;

		void BeginFrame() override;
		void PresentFrame() override;
		void WaitIdle();

		void SubmitResourceFree(std::function<void()> fn, bool freeOnShutdown = true);
		void RunGC(uint32_t frameNumber);

		Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		Ref<VulkanLogicalDevice> GetLogicalDevice() const { return m_LogicalDevice; }
		Ref<VulkanSwapchain> GetSwapchain() const { return m_Swapchain; }

		static VkInstance GetVulkanInstance() { return s_Instance; }
		GLFWwindow* GetWindowHandle() override { return m_WindowHandle; }

		static Ref<VulkanContext> Get();

	private:
		std::vector<const char*> GetRequiredExtensions();

	private:
		GLFWwindow* m_WindowHandle = nullptr;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		Ref<VulkanLogicalDevice> m_LogicalDevice;
		Ref<VulkanSwapchain> m_Swapchain;

		GarbageCollector m_GarbageCollector;

		inline static VkInstance s_Instance;
	};
}
