#pragma once

#include "Platform/Vulkan/LogicalDevice.h"
#include "Platform/Vulkan/PhysicalDevice.h"
#include "Platform/Vulkan/Swapchain.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/DeviceManager.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/validation.h>
#include <nvrhi/vulkan.h>

namespace Eppo
{
	class DeviceManagerVK : public DeviceManager
	{
	public:
		DeviceManagerVK(const Ref<Window>& window, const DeviceParams& params);
        ~DeviceManagerVK() override = default;

		auto Init() -> void override;
		auto Shutdown() -> void override;

		[[nodiscard]] auto GetDevice() const -> nvrhi::IDevice* override;

		auto BeginFrame() -> bool override;
		auto Present() -> bool override;

		[[nodiscard]] auto GetCurrentBackBufferIndex() const -> uint32_t override { return m_Swapchain->GetCurrentBackBufferIndex(); }
		[[nodiscard]] auto GetBackBufferCount() const -> uint32_t override { return m_Swapchain->GetImageCount(); }
		auto GetCurrentSwapchainImage() -> const SwapchainImage& override { return m_Swapchain->GetCurrentSwapchainImage(); }

		[[nodiscard]] constexpr auto GetVulkanInstance() const -> VkInstance { return m_Instance; }
		[[nodiscard]] constexpr auto GetPhysicalDevice() const -> const ScopedPtr<PhysicalDevice>& { return m_PhysicalDevice; }
		[[nodiscard]] constexpr auto GetLogicalDevice() const -> const ScopedPtr<LogicalDevice>& { return m_LogicalDevice; }
		[[nodiscard]] constexpr auto GetSwapchain() const -> const ScopedPtr<Swapchain>& { return m_Swapchain; }

	private:
		auto CreateVulkanInstance() -> void;
		auto CreateNvrhiDevice() -> void;

	private:
		nvrhi::vulkan::DeviceHandle m_Device = nullptr;
		nvrhi::DeviceHandle m_ValidationLayer = nullptr;

		VkInstance m_Instance = nullptr;
		VkDebugUtilsMessengerEXT m_DebugMessenger = nullptr;

		ScopedPtr<PhysicalDevice> m_PhysicalDevice = nullptr;
		ScopedPtr<LogicalDevice> m_LogicalDevice = nullptr;
		ScopedPtr<Swapchain> m_Swapchain = nullptr;
	};
}
