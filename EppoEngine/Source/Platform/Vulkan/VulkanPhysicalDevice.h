#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Image.h"

namespace Eppo
{
    struct QueueFamilyIndices
    {
        int32_t Graphics = -1;
        int32_t Present = -1;

        [[nodiscard]] bool IsComplete() const
        {
            return Graphics > -1 && Present > -1;
        }
    };

    class VulkanPhysicalDevice final
    {
    public:
        VulkanPhysicalDevice();
        VulkanPhysicalDevice(const VulkanPhysicalDevice&) = delete;
        VulkanPhysicalDevice(const VulkanPhysicalDevice&&) = delete;
        VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice&) = delete;
        VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice&&) = delete;
        ~VulkanPhysicalDevice() = default;

        [[nodiscard]] VkPhysicalDevice GetNativeDevice() const { return m_PhysicalDevice; }
        [[nodiscard]] VkSurfaceKHR GetSurface() const { return m_Surface; }

        [[nodiscard]] QueueFamilyIndices& GetQueueFamilyIndices() { return m_QueueFamilyIndices; }
        [[nodiscard]] const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

        [[nodiscard]] const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_Properties; }
        [[nodiscard]] const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const { return m_MemoryProperties; }
        [[nodiscard]] const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_Features; }

        [[nodiscard]] VkFormat GetSupportedImageFormat(const ImageFormat format) { return m_SupportedImageFormats[format]; }
        bool IsExtensionSupported(std::string_view extension);

    private:
        [[nodiscard]] QueueFamilyIndices FindQueueFamilyIndices() const;

    private:
        VkPhysicalDevice m_PhysicalDevice;
        VkPhysicalDeviceProperties m_Properties;
        VkPhysicalDeviceMemoryProperties m_MemoryProperties;
        VkPhysicalDeviceFeatures m_Features;

        VkSurfaceKHR m_Surface;

        std::unordered_map<ImageFormat, VkFormat> m_SupportedImageFormats;

        QueueFamilyIndices m_QueueFamilyIndices;
        std::vector<std::string> m_SupportedExtensions;
    };
}
