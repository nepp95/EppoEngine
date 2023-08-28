#pragma once

#include "Renderer/LogicalDevice.h"

namespace Eppo
{
	class RendererContext;

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class Swapchain
	{
	public:
		Swapchain();

		void BeginFrame();
		void Present();

		void Cleanup();
		void Create(bool recreate = false);
		void Destroy();

		void OnResize();

		VkCommandBuffer GetCurrentRenderCommandBuffer() { return m_CommandBuffers[m_CurrentFrameIndex]; }
		uint32_t GetCurrentImageIndex() const { return m_CurrentImageIndex; }

		VkRenderPass GetRenderPass() { return m_RenderPass; }
		VkFramebuffer GetCurrentFramebuffer() { return m_Framebuffers[m_CurrentFrameIndex]; }

		VkExtent2D GetExtent() const { return m_Extent;	};
		uint32_t GetWidth() const { return m_Extent.width; }
		uint32_t GetHeight() const { return m_Extent.height; }

	private:
		SwapchainSupportDetails QuerySwapchainSupport(Ref<RendererContext> context);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	private:
		VkSwapchainKHR m_Swapchain;
		VkSurfaceKHR m_Surface;

		VkExtent2D m_Extent;
		VkFormat m_ImageFormat;
		uint32_t m_ImageCount;
		uint32_t m_CurrentImageIndex = 0;
		uint32_t m_CurrentFrameIndex = 0;

		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		std::vector<VkFramebuffer> m_Framebuffers;
		VkRenderPass m_RenderPass;

		std::vector<VkCommandPool> m_CommandPools;
		std::vector<VkCommandBuffer> m_CommandBuffers;

		std::vector<VkSemaphore> m_PresentSemaphores;
		std::vector<VkSemaphore> m_RenderSemaphores;
		std::vector<VkFence> m_Fences;

		friend class RendererContext;
	};
}
