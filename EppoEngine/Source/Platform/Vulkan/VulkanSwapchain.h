#pragma once

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanLogicalDevice.h"

struct GLFWwindow;

namespace Eppo
{
	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class VulkanSwapchain
	{
	public:
		VulkanSwapchain(Ref<VulkanLogicalDevice> logicalDevice);

		void Create(bool recreate = false);

		void Cleanup();
		void Destroy();

		void BeginFrame();
		void PresentFrame();

		void OnResize();

		VkImage GetCurrentImage() const { return m_Images[m_CurrentFrameIndex]; }
		VkImageView GetCurrentImageView() const { return m_ImageViews[m_CurrentFrameIndex]; }
		uint32_t GetCurrentImageIndex() const { return m_CurrentImageIndex; }

		uint32_t GetWidth() const { return m_Extent.width; }
		uint32_t GetHeight() const { return m_Extent.height; }
		VkExtent2D GetExtent() const { return m_Extent; }

		Ref<VulkanCommandBuffer> GetCommandBuffer() const { return m_CommandBuffer; }

	private:
		SwapchainSupportDetails QuerySwapchainSupportDetails(const Ref<VulkanPhysicalDevice>& physicalDevice);

		VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
		VkPresentModeKHR SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
		VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	private:
		Ref<VulkanLogicalDevice> m_LogicalDevice;
		Ref<VulkanCommandBuffer> m_CommandBuffer;

		VkSwapchainKHR m_Swapchain;
		VkExtent2D m_Extent;
		VkFormat m_ImageFormat;

		uint32_t m_CurrentFrameIndex = 0;
		uint32_t m_CurrentImageIndex = 0;
		uint32_t m_FrameCounter = 0;

		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		std::vector<VkFramebuffer> m_Framebuffers;

		std::vector<VkFence> m_Fences;
		std::vector<VkSemaphore> m_PresentSemaphores;
		std::vector<VkSemaphore> m_RenderSemaphores;
	};
}
