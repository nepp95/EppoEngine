#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/DeviceManager.h"

#include <queue>

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
		Swapchain(VkSurfaceKHR surface);
		~Swapchain();

		auto BeginFrame() -> bool;
		auto Present() -> bool;

		auto CreateSwapchain(uint32_t width = 0, uint32_t height = 0) -> void;
		auto Resize(uint32_t width, uint32_t height) -> void;

		auto GetCurrentBackBufferIndex() const -> uint32_t { return m_SwapchainIndex; }
		auto GetImageCount() const -> uint32_t { return static_cast<uint32_t>(m_Images.size()); }
		auto GetCurrentSwapchainImage() -> const SwapchainImage& { return m_Images.at(m_SwapchainIndex); }

	private:
		[[nodiscard]] auto QuerySwapchainSupportDetails() const -> SwapchainSupportDetails;
		[[nodiscard]] auto SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) -> VkSurfaceFormatKHR;
		[[nodiscard]] auto SelectPresentMode(const std::vector<VkPresentModeKHR>& presentModes) -> VkPresentModeKHR;
		[[nodiscard]] auto SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities) const -> VkExtent2D;

	private:
		VkSwapchainKHR m_Swapchain = nullptr;
		VkSurfaceKHR m_Surface = nullptr;

		std::vector<SwapchainImage> m_Images;

		VkFormat m_Format = VK_FORMAT_UNDEFINED;
		VkExtent2D m_Extent{};
		VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
		VkSurfaceFormatKHR m_SurfaceFormat;

		std::array<VkSemaphore, g_MaxFramesInFlight> m_AcquireSemaphores{};
		std::vector<VkSemaphore> m_PresentSemaphores;
		std::queue<nvrhi::EventQueryHandle> m_FramesInFlight;
		std::vector<nvrhi::EventQueryHandle> m_QueryPool;

		uint32_t m_AcquireIndex = 0;
		uint32_t m_SwapchainIndex = 0;
	};
}
