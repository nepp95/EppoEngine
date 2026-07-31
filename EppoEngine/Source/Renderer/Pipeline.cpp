#include "pch.h"
#include "Renderer/Pipeline.h"
#include "Renderer/DeviceManager.h"

namespace Eppo
{
    Pipeline::Pipeline(PipelineSpecification spec, const nvrhi::FramebufferInfo& framebufferInfo)
        : m_Specification(std::move(spec))
    {
        const auto& dm = DeviceManager::Get();
        auto device = dm->GetDevice();

        nvrhi::RasterState rasterState{
            .fillMode = m_Specification.FillMode,
            .cullMode = m_Specification.CullMode,
            .depthBias = m_Specification.DepthBias,
            .slopeScaledDepthBias = m_Specification.SlopeScaledDepthBias,
        };

        nvrhi::DepthStencilState depthStencilState{
            .depthTestEnable = m_Specification.DepthTestEnable,
            .depthWriteEnable = m_Specification.DepthWriteEnable,
            .depthFunc = m_Specification.DepthFunc,
        };

        nvrhi::RenderState renderState{
            .blendState = m_Specification.BlendState,
            .depthStencilState = depthStencilState,
            .rasterState = rasterState,
        };

        nvrhi::GraphicsPipelineDesc pipelineDesc{
            .inputLayout = m_Specification.Shader->GetInputLayout(),
            .VS = m_Specification.Shader->GetShaderHandle(nvrhi::ShaderType::Vertex),
            .PS = m_Specification.Shader->GetShaderHandle(nvrhi::ShaderType::Pixel),
            .renderState = renderState,
        };

        uint32_t expectedSet = 0;
        for (const auto& [set, layout] : m_Specification.Shader->GetBindingLayouts())
        {
            EP_ASSERT(set == expectedSet);
            ++expectedSet;
            if (layout)
                pipelineDesc.addBindingLayout(layout);
        }

        m_PipelineHandle = device->createGraphicsPipeline(pipelineDesc, framebufferInfo);
        EP_ASSERT(m_PipelineHandle);
    }

    auto Pipeline::IsCompatible(const Ref<Framebuffer>& framebuffer) const -> bool
    {
        if (!m_PipelineHandle || !framebuffer)
        {
            Log::Warn("Pipeline::IsCompatible called but either pipeline or framebuffer is null!");
            return false;
        }

        const auto& pipelineFbInfo = m_PipelineHandle->getFramebufferInfo();
        const auto& fbInfo = framebuffer->GetFramebuffer()->getFramebufferInfo();

        return pipelineFbInfo == fbInfo;
    }
}
