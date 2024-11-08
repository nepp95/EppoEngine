#pragma once

#include "Renderer/LogicalDevice.h"
#include "Renderer/RenderCommandBuffer.h"

struct GLFWwindow;

namespace Eppo
{
	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class Swapchain
	{
	public:
		Swapchain(Ref<LogicalDevice> logicalDevice);

		void Create(bool recreate = false);

		void BeginFrame();
		void Present();

		void OnResize();

		VkImage GetCurrentImage() const { return m_Images[m_CurrentFrameIndex]; }
		VkImageView GetCurrentImageView() const { return m_ImageViews[m_CurrentFrameIndex]; }
		uint32_t GetCurrentImageIndex() const { return m_CurrentImageIndex; }

		uint32_t GetWidth() const { return m_Extent.width; }
		uint32_t GetHeight() const { return m_Extent.height; }
		VkExtent2D GetExtent() const { return m_Extent; }

		Ref<RenderCommandBuffer> GetCommandBuffer() const { return m_CommandBuffer; }

	private:
		SwapchainSupportDetails QuerySwapchainSupportDetails(const Ref<PhysicalDevice>& physicalDevice);

		VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
		VkPresentModeKHR SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
		VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	private:
		Ref<LogicalDevice> m_LogicalDevice;
		Ref<RenderCommandBuffer> m_CommandBuffer;

		VkSwapchainKHR m_Swapchain;
		VkExtent2D m_Extent;
		VkFormat m_ImageFormat;

		uint32_t m_CurrentFrameIndex = 0;
		uint32_t m_CurrentImageIndex = 0;

		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		std::vector<VkFramebuffer> m_Framebuffers;

		std::vector<VkFence> m_Fences;
		std::vector<VkSemaphore> m_PresentSemaphores;
		std::vector<VkSemaphore> m_RenderSemaphores;
	};
}
