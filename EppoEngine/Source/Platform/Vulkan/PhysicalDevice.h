#pragma once

#include "Platform/Vulkan/Vulkan.h"

namespace Eppo
{
    struct QueueFamilyIndices
    {
        int32_t Graphics = -1;
        int32_t Compute = -1;
        int32_t Transfer = -1;
        int32_t Present = -1;

        [[nodiscard]] constexpr auto IsComplete() const -> bool { return Graphics > -1 && Compute > -1 && Transfer > -1 && Present > -1; }

        [[nodiscard]] auto GetUniqueIndices() const -> std::unordered_set<int32_t> { return { Graphics, Compute, Transfer, Present }; }
    };

    class PhysicalDevice
    {
    public:
        PhysicalDevice(VkInstance instance);

        [[nodiscard]] constexpr auto GetNative() const -> VkPhysicalDevice { return m_Device; }
        [[nodiscard]] constexpr auto GetDeviceProperties() const -> const VkPhysicalDeviceProperties& { return m_Properties; }
        [[nodiscard]] constexpr auto GetDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties&
        {
            return m_MemoryProperties;
        }
        [[nodiscard]] constexpr auto GetDeviceFeatures() -> VkPhysicalDeviceFeatures2& { return m_Features; }
        [[nodiscard]] constexpr auto GetQueueFamilyIndices() const -> const QueueFamilyIndices& { return m_QueueFamilyIndices; }

        [[nodiscard]] auto IsExtensionSupported(std::string_view extensionName) -> bool;

    private:
        VkPhysicalDevice m_Device = nullptr;
        VkPhysicalDeviceProperties m_Properties{};
        VkPhysicalDeviceMemoryProperties m_MemoryProperties{};
        VkPhysicalDeviceFeatures2 m_Features{};

        QueueFamilyIndices m_QueueFamilyIndices;
        std::vector<std::string> m_SupportedExtensions;
    };
}