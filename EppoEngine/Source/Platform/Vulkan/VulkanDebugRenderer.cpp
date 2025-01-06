#include "pch.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanDebugRenderer.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
    void VulkanDebugRenderer::StartDebugLabel(const Ref<CommandBuffer>& commandBuffer, const std::string& label)
    {
        const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
        const VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

        VkDebugUtilsLabelEXT debugLabel{};
        debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        debugLabel.pLabelName = label.c_str();
        debugLabel.color[0] = 0.5f;
        debugLabel.color[1] = 0.5f;
        debugLabel.color[2] = 0.0f;
        debugLabel.color[3] = 1.0f;

        vkCmdBeginDebugUtilsLabelEXT(cb, &debugLabel);
    }

    void VulkanDebugRenderer::EndDebugLabel(const Ref<CommandBuffer>& commandBuffer)
    {
        const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
        const VkCommandBuffer cb = cmd->GetCurrentCommandBuffer();

        vkCmdEndDebugUtilsLabelEXT(cb);
    }
}
