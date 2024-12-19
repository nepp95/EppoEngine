#pragma once

#include "Platform/Vulkan/DescriptorLayoutBuilder.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Platform/Vulkan/VulkanLogicalDevice.h"
#include "Platform/Vulkan/VulkanPhysicalDevice.h"
#include "Platform/Vulkan/VulkanRenderer.h"
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

		[[nodiscard]] uint32_t GetCurrentFrameIndex() const override { return m_Swapchain->GetCurrentImageIndex(); }
		void BeginFrame() override;
		void PresentFrame() override;
		void WaitIdle() override;

		void SubmitResourceFree(const std::function<void()>& fn, bool freeOnShutdown = true);
		void RunGC(uint32_t frameNumber);

		[[nodiscard]] Ref<VulkanLogicalDevice> GetLogicalDevice() const { return m_LogicalDevice; }
		[[nodiscard]] Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		[[nodiscard]] Ref<VulkanSwapchain> GetSwapchain() const { return m_Swapchain; }

		[[nodiscard]] Ref<Renderer> GetRenderer() const override { return m_Renderer; }

		DescriptorLayoutBuilder& GetDescriptorLayoutBuilder() { return m_DescriptorLayoutBuilder; }

		static VkInstance GetVulkanInstance() { return s_Instance; }
		GLFWwindow* GetWindowHandle() override { return m_WindowHandle; }
		[[nodiscard]] TracyVkCtx GetTracyContext() const { return m_TracyContext; }

		static Ref<VulkanContext> Get();

	private:
		[[nodiscard]] std::vector<const char*> GetRequiredExtensions() const;

	private:
		GLFWwindow* m_WindowHandle = nullptr;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		Ref<VulkanLogicalDevice> m_LogicalDevice;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		Ref<VulkanRenderer> m_Renderer;
		Ref<VulkanSwapchain> m_Swapchain;

		DescriptorLayoutBuilder m_DescriptorLayoutBuilder;
		GarbageCollector m_GarbageCollector;

		TracyVkCtx m_TracyContext;

		inline static VkInstance s_Instance;
	};
}
