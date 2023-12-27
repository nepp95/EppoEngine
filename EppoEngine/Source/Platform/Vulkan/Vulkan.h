#pragma once

#include <vulkan/vulkan.h>

namespace Eppo
{
	struct VulkanConfig
	{
		#if defined(EPPO_DEBUG) || defined(EPPO_RELEASE)
			inline static const bool EnableValidation = true;
		#else
			inline static const bool EnableValidation = false;
		#endif
	
		inline static const uint32_t MaxFramesInFlight = 2;
		inline static const std::vector<const char*> ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
		inline static const std::vector<const char*> DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	};
}
