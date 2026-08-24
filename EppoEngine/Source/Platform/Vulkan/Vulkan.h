#pragma once

#include <vulkan/vulkan.h>

namespace Eppo
{
#define VK_CHECK(fn, msg)                                                                                                                  \
    if (fn != VK_SUCCESS)                                                                                                                  \
        Log::Error(LogSource::Vulkan, msg);

#if !defined(EP_DIST)
    constexpr bool g_EnableValidationLayers = true;
#else
    constexpr bool g_EnableValidationLayers = false;
#endif

    constexpr std::array g_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
    constexpr std::array g_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
                                                VK_GOOGLE_HLSL_FUNCTIONALITY_1_EXTENSION_NAME, VK_GOOGLE_USER_TYPE_EXTENSION_NAME,
                                                VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME };

    [[maybe_unused]]
    static VKAPI_ATTR auto VKAPI_CALL VulkanDebugCallback(
        const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
    ) -> VkBool32
    {
        /*
        MessageSeverity:
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic message
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: Informational message like the creation of a resource
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: Message about behavior that is not necessarily an error, but very likely a bug
        in your application
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: Message about behavior that is invalid and may cause crashes

        MessageType:
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event has happened that is unrelated to the specification or performance
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: Something has happened that violates the specification or indicates a possible
        mistake VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: Potential non-optimal use of Vulkan
        */

        switch (messageSeverity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            {
                Log::Trace(LogSource::Vulkan, "{}", pCallbackData->pMessage);
                break;
            }

            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            {
                Log::Info(LogSource::Vulkan, "{}", pCallbackData->pMessage);
                break;
            }

            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            {
                Log::Warn(LogSource::Vulkan, "{}", pCallbackData->pMessage);
                break;
            }

            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            {
                Log::Error(LogSource::Vulkan, "{}", pCallbackData->pMessage);
                break;
            }

            default:
                break;
        }

        return false;
    }

    [[maybe_unused]]
    static auto CreateDebugUtilsMessengerEXT(
        VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    ) -> VkResult
    {
        if (const auto fn =
                reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            fn != nullptr)
        {
            return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    [[maybe_unused]]
    static auto
    DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
        -> void
    {
        if (const auto fn =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            fn != nullptr)
        {
            return fn(instance, debugMessenger, pAllocator);
        }
    }
}
