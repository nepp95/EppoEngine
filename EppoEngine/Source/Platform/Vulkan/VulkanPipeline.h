#pragma once

#include "Platform/Vulkan/VulkanCmd.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Image.h"
#include "Renderer/Pipeline.h"

namespace Eppo
{
    class VulkanPipeline final : public Pipeline
    {
    public:
        explicit VulkanPipeline(PipelineSpecification specification);
        ~VulkanPipeline() override;

        [[nodiscard]] Ref<VulkanCmd> GetCommandBuffers() const { return m_CommandBuffers; }

        [[nodiscard]] Ref<Image> GetImage(const uint32_t index) const override
        {
            return m_Specification.RenderAttachments.at(index).RenderImage;
        }

        [[nodiscard]] Ref<Image> GetFinalImage() const override { return m_Specification.RenderAttachments.at(0).RenderImage; }

        [[nodiscard]] VkPipeline GetPipeline() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

        [[nodiscard]] const PipelineSpecification& GetSpecification() const override { return m_Specification; }
        PipelineSpecification& GetSpecification() override { return m_Specification; }

    private:
        PipelineSpecification m_Specification;
        Ref<VulkanCmd> m_CommandBuffers;

        VkPipeline m_Pipeline;
        VkPipelineLayout m_PipelineLayout;
    };
}
