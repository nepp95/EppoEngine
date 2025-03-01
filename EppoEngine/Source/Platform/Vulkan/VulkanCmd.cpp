#include "pch.h"
#include "VulkanCmd.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Eppo
{
    namespace
    {
        bool s_QueryPoolCreated = false;
        uint32_t s_QueryIndex = 2;
        uint32_t s_QueryCount = 2;
        std::array<VkQueryPool, VulkanConfig::MaxFramesInFlight> s_QueryPools;
    }

    VulkanCmd::VulkanCmd()
        : m_QueryIndex(s_QueryIndex)
    {
        s_QueryIndex += 2;

        const auto context = VulkanContext::Get();
        const auto logicalDevice = context->GetLogicalDevice();
        const VkDevice device = logicalDevice->GetNativeDevice();

        // Create command pool
        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = context->GetPhysicalDevice()->GetQueueFamilyIndices().Graphics;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "Failed to create command pool!");

        // Allocate command buffers
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = m_CommandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = VulkanConfig::MaxFramesInFlight;

        VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, m_CommandBuffers.data()),
                 "Failed to allocate command buffers!");

        // Create fences
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (auto& fence : m_Fences)
            VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence), "Failed to create fence!");

        // Create query pools
        if (!s_QueryPoolCreated)
        {
            VkQueryPoolCreateInfo queryPoolCreateInfo{};
            queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolCreateInfo.queryCount = s_QueryCount;

            for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
                VK_CHECK(vkCreateQueryPool(device, &queryPoolCreateInfo, nullptr, &s_QueryPools[i]), "Failed to create query pool!");

            s_QueryPoolCreated = true;
        }

        // Graciously release resources at the end of our application
        context->SubmitResourceFree([this, device]()
        {
            for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
            {
                if (s_QueryPoolCreated)
                    vkDestroyQueryPool(device, s_QueryPools[i], nullptr);
                vkDestroyFence(device, m_Fences[i], nullptr);
            }

            s_QueryPoolCreated = false;
            vkDestroyCommandPool(device, m_CommandPool, nullptr);
        });
    }

    void VulkanCmd::RT_Begin()
    {
        const uint32_t imageIndex = VulkanContext::Get()->GetCurrentFrameIndex();

        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pNext = nullptr;

        VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[imageIndex], &beginInfo), "Failed to begin command buffer!");
        vkCmdResetQueryPool(m_CommandBuffers[imageIndex], s_QueryPools[imageIndex], m_QueryIndex, 2);
        vkCmdWriteTimestamp2(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, s_QueryPools[imageIndex], m_QueryIndex);
    }

    void VulkanCmd::RT_End()
    {
        const uint32_t imageIndex = VulkanContext::Get()->GetCurrentFrameIndex();

        vkCmdWriteTimestamp2(m_CommandBuffers[imageIndex], VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, s_QueryPools[imageIndex],
                             m_QueryIndex + 1);
        VK_CHECK(vkEndCommandBuffer(m_CommandBuffers[imageIndex]), "Failed to end command buffer!");
    }

    void VulkanCmd::SetTotalQueryCount(const uint32_t queryCount)
    {
        s_QueryCount = queryCount;
    }

    VkCommandBuffer VulkanCmd::GetCurrentCommandBuffer() const
    {
        const uint32_t imageIndex = VulkanContext::Get()->GetCurrentFrameIndex();
        EPPO_ASSERT(m_CommandBuffers.size() > imageIndex);
        return m_CommandBuffers[imageIndex];
    }
}
