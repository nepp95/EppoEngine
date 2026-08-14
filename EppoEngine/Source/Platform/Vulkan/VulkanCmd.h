#pragma once

#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/CommandBuffer.h"

namespace Eppo
{
    class VulkanCmd final : public CommandBuffer
    {
    public:
        VulkanCmd();
        VulkanCmd(const VulkanCmd&) = delete;
        VulkanCmd(const VulkanCmd&&) = delete;
        VulkanCmd& operator=(const VulkanCmd&) = delete;
        VulkanCmd& operator=(const VulkanCmd&&) = delete;
        ~VulkanCmd() override = default;

        void RT_Begin() override;
        void RT_End() override;

        static void SetTotalQueryCount(uint32_t queryCount);
        [[nodiscard]] uint32_t GetQueryIndex() const { return m_QueryIndex; }

        [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const;

    private:
        VkCommandPool m_CommandPool;
        std::array<VkCommandBuffer, VulkanConfig::MaxFramesInFlight> m_CommandBuffers;
        std::array<VkFence, VulkanConfig::MaxFramesInFlight> m_Fences;

        uint32_t m_QueryIndex;
    };
}
