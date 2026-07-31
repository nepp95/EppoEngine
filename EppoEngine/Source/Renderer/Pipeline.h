#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"

namespace Eppo
{
    struct PipelineSpecification
    {
        Ref<Shader> Shader;

        // Rasterization
        nvrhi::RasterCullMode CullMode = nvrhi::RasterCullMode::Back;
        nvrhi::RasterFillMode FillMode = nvrhi::RasterFillMode::Solid;

        // Depth Stencil
        bool DepthTestEnable = false;
        bool DepthWriteEnable = false;
        nvrhi::ComparisonFunc DepthFunc = nvrhi::ComparisonFunc::Less;
        nvrhi::BlendState BlendState{};

        // Depth bias (polygon offset), in Vulkan units — pulls geometry toward
        // (negative) or away from (positive) the camera to resolve z-fighting.
        int DepthBias = 0;
        float SlopeScaledDepthBias = 0.f;
    };

    class Pipeline
    {
    public:
        Pipeline(PipelineSpecification spec, const nvrhi::FramebufferInfo& framebufferInfo);

        [[nodiscard]] auto IsCompatible(const Ref<Framebuffer>& framebuffer) const -> bool;

        [[nodiscard]] constexpr auto GetSpecification() const -> const PipelineSpecification& { return m_Specification; }
        [[nodiscard]] auto GetPipeline() const -> nvrhi::GraphicsPipelineHandle { return m_PipelineHandle; }

    private:
        PipelineSpecification m_Specification;
        nvrhi::GraphicsPipelineHandle m_PipelineHandle = nullptr;
    };
}
