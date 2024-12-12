#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Platform/Vulkan/VulkanLogicalDevice.h"
#include "Platform/Vulkan/VulkanPhysicalDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Renderer/GarbageCollector.h"
#include "Renderer/RendererContext.h"

#include <tracy/TracyVulkan.hpp>

namespace Eppo
{
	class VulkanContext : public RendererContext
	{
	public:
		explicit VulkanContext(GLFWwindow* windowHandle);
		~VulkanContext() override = default;

		void Init() override;
		void Shutdown() override;

		void BeginFrame() override;
		void PresentFrame() override;
		void WaitIdle() override;

		void SubmitResourceFree(std::function<void()> fn, bool freeOnShutdown = true);
		void RunGC(uint32_t frameNumber);

		Ref<VulkanLogicalDevice> GetLogicalDevice() const { return m_LogicalDevice; }
		Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		Ref<VulkanSwapchain> GetSwapchain() const { return m_Swapchain; }

		static VkInstance GetVulkanInstance() { return s_Instance; }
		GLFWwindow* GetWindowHandle() override { return m_WindowHandle; }
		TracyVkCtx GetTracyContext() const { return m_TracyContext; }

		static Ref<VulkanContext> Get();

	private:
		std::vector<const char*> GetRequiredExtensions() const;

	private:
		GLFWwindow* m_WindowHandle = nullptr;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		Ref<VulkanLogicalDevice> m_LogicalDevice;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		Ref<VulkanSwapchain> m_Swapchain;

		GarbageCollector m_GarbageCollector;

		TracyVkCtx m_TracyContext;

		inline static VkInstance s_Instance;
	};
}
