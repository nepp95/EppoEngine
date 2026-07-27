#pragma once

#include "Platform/Vulkan/PhysicalDevice.h"

namespace Eppo
{
    class LogicalDevice
    {
    public:
        LogicalDevice(const ScopedPtr<PhysicalDevice>& physicalDevice);
        ~LogicalDevice();

        [[nodiscard]] constexpr auto GetNative() const -> VkDevice { return m_Device; }
        [[nodiscard]] constexpr auto GetComputeQueue() const -> VkQueue { return m_ComputeQueue; }
        [[nodiscard]] constexpr auto GetGraphicsQueue() const -> VkQueue { return m_GraphicsQueue; }
        [[nodiscard]] constexpr auto GetPresentQueue() const -> VkQueue { return m_PresentQueue; }
        [[nodiscard]] constexpr auto GetTransferQueue() const -> VkQueue { return m_TransferQueue; }

    private:
        VkDevice m_Device = nullptr;

        VkQueue m_ComputeQueue = nullptr;
        VkQueue m_GraphicsQueue = nullptr;
        VkQueue m_PresentQueue = nullptr;
        VkQueue m_TransferQueue = nullptr;
    };
}