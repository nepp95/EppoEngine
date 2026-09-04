#include "pch.h"
#include "Platform/Vulkan/VulkanGpuProfiler.h"

#include "Platform/Vulkan/DeviceManagerVK.h"
#include "Renderer/RenderCommandBuffer.h"

namespace Eppo
{
    VulkanGpuProfiler::VulkanGpuProfiler()
    {
#if defined(TRACY_ENABLE)
        const auto dm = std::static_pointer_cast<DeviceManagerVK>(DeviceManager::Get());

        const VkPhysicalDevice physicalDevice = dm->GetPhysicalDevice()->GetNative();
        const VkDevice device = dm->GetLogicalDevice()->GetNative();
        const VkQueue graphicsQueue = dm->GetLogicalDevice()->GetGraphicsQueue();
        const auto& indices = dm->GetPhysicalDevice()->GetQueueFamilyIndices();

        // TracyVkContext owns and re-begins its setup buffer, so give it a dedicated one from a reset-capable pool, not an nvrhi list.
        const VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = static_cast<uint32_t>(indices.Graphics),
        };
        VkCommandPool setupPool = nullptr;
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &setupPool), "Failed to create Tracy setup command pool!");

        const VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = setupPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer commandBuffer = nullptr;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "Failed to allocate Tracy setup command buffer!");

        m_Context = TracyVkContext(physicalDevice, device, graphicsQueue, commandBuffer);

        vkFreeCommandBuffers(device, setupPool, 1, &commandBuffer);
        vkDestroyCommandPool(device, setupPool, nullptr);
#endif
    }

    VulkanGpuProfiler::~VulkanGpuProfiler()
    {
#if defined(TRACY_ENABLE)
        if (m_Context)
            TracyVkDestroy(m_Context);
#endif
    }

    auto VulkanGpuProfiler::BeginZone(
        [[maybe_unused]] const Ref<RenderCommandBuffer>& commandBuffer, [[maybe_unused]] const char* name,
        [[maybe_unused]] const char* function, [[maybe_unused]] const char* file, [[maybe_unused]] const uint32_t line
    ) -> void
    {
#if defined(TRACY_ENABLE)
        const auto cmd =
            static_cast<VkCommandBuffer>(commandBuffer->GetCommandList()->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer));
        m_Zone.emplace(m_Context, line, file, strlen(file), function, strlen(function), name, strlen(name), cmd, true);
#endif
    }

    auto VulkanGpuProfiler::EndZone() -> void
    {
#if defined(TRACY_ENABLE)
        m_Zone.reset();
#endif
    }

    auto VulkanGpuProfiler::Collect([[maybe_unused]] const Ref<RenderCommandBuffer>& commandBuffer) -> void
    {
#if defined(TRACY_ENABLE)
        const auto cmd =
            static_cast<VkCommandBuffer>(commandBuffer->GetCommandList()->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer));
        TracyVkCollect(m_Context, cmd);
#endif
    }

}
