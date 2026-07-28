#include "pch.h"
#include "Renderer/Pipeline.h"
#include "Renderer/DeviceManager.h"

namespace Eppo
{
    Pipeline::Pipeline(PipelineSpecification spec)
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

        m_PipelineHandle =
            device->createGraphicsPipeline(pipelineDesc, m_Specification.Framebuffer->GetFramebuffer()->getFramebufferInfo());
        EP_ASSERT(m_PipelineHandle);
    }

    auto Pipeline::Resize(const uint32_t width, const uint32_t height) const -> void
    {
        if (m_Specification.OwnsFramebuffer)
            m_Specification.Framebuffer->Resize(width, height);
    }
}
