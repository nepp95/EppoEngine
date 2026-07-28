#include "pch.h"
#include "Renderer/Renderer.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Sampler.h"

#include <nvrhi/utils.h>
#include <nvrhi/nvrhi.h>

namespace Eppo
{
    namespace
    {
        constexpr std::array s_EngineShaderNames{ "composite", "geometry", "imgui", "skybox", "wireframe" };
    }

    Renderer::Renderer()
    {
        m_DescriptorManager = CreateRef<DescriptorManager>();
    }

    auto Renderer::LoadShaders(const std::map<std::string, PackedShaderData>& packed, const std::map<std::string, std::string>& includes)
        -> void
    {
        for (const auto* name : s_EngineShaderNames)
        {
            ShaderSpecification spec{ .Name = name };
            if (!packed.empty())
            {
                const auto it = packed.find(name);
                // An entry without sources would be compiled from disk, which a packaged game does not have.
                if (it == packed.end() || it->second.ShaderSources.empty())
                {
                    Log::Error("Shader '{}' is missing from the game package.", name);
                    EP_ASSERT(false, "Incomplete game package!");
                    continue;
                }

                spec.Sources = it->second.ShaderSources;
                spec.Includes = includes;
            }

            m_ShaderLibrary.Load(std::move(spec));
        }
    }

    auto Renderer::Init() -> void
    {
        m_CompositeSampler = Sampler::Create({ .AddressMode = nvrhi::SamplerAddressMode::Clamp });

        m_CompositeCommandBuffer = CreateRef<RenderCommandBuffer>();
    }

    auto Renderer::BeginRenderPass(const Ref<RenderCommandBuffer>& commandBuffer, const Ref<RenderPass>& renderPass) -> void
    {
        const auto& cmd = commandBuffer->GetCommandList();
        const auto& pipeline = renderPass->GetPipeline();
        const auto& framebuffer = pipeline->GetSpecification().Framebuffer;
        const auto& renderPassSpec = renderPass->GetSpecification();

        cmd->beginMarker(renderPassSpec.Name.c_str());

        if (renderPassSpec.ClearColorOnLoad)
        {
            const auto& clearColor = renderPassSpec.ClearColor;
            for (size_t i = 0; i < framebuffer->GetFramebuffer()->getDesc().colorAttachments.size(); i++)
                nvrhi::utils::ClearColorAttachment(
                    cmd, framebuffer->GetFramebuffer(), i, nvrhi::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a)
                );
        }

        if (renderPassSpec.ClearDepthOnLoad)
            nvrhi::utils::ClearDepthStencilAttachment(
                cmd, framebuffer->GetFramebuffer(), renderPassSpec.DepthClearValue, renderPassSpec.StencilClearValue
            );

        auto& graphicsState = commandBuffer->GetGraphicsState();
        graphicsState.pipeline = pipeline->GetPipeline();
        EP_ASSERT(graphicsState.pipeline != nullptr);
        graphicsState.framebuffer = framebuffer->GetFramebuffer();
        EP_ASSERT(graphicsState.framebuffer != nullptr);

        graphicsState.viewport.viewports = {
            nvrhi::Viewport(static_cast<float>(framebuffer->GetWidth()), static_cast<float>(framebuffer->GetHeight()))
        };
        graphicsState.viewport.scissorRects = {
            nvrhi::Rect(static_cast<int>(framebuffer->GetWidth()), static_cast<int>(framebuffer->GetHeight()))
        };

        graphicsState.bindings = renderPass->GetBindingSets();
        EP_ASSERT(graphicsState.bindings.size() == pipeline->GetPipeline()->getDesc().bindingLayouts.size());

        commandBuffer->CommitGraphicsState();
    }

    auto Renderer::EndRenderPass(const Ref<RenderCommandBuffer>& commandBuffer) -> void
    {
        commandBuffer->GetCommandList()->endMarker();
    }

    auto Renderer::CompositeToSwapchain(const Ref<Image>& image) -> void
    {
        EP_PROFILE_FN("Renderer::CompositeToSwapchain")

        EP_ASSERT(image != nullptr, "Cannot composite a null image to the swapchain.");

        const auto& dm = DeviceManager::Get();
        const uint32_t backBufferCount = dm->GetBackBufferCount();
        const uint32_t backBufferIndex = dm->GetCurrentBackBufferIndex();
        EP_ASSERT(backBufferIndex < backBufferCount, "The current swapchain back buffer index is invalid.");

        if (m_CompositePasses.size() != backBufferCount)
        {
            m_CompositePasses.resize(backBufferCount);
            m_CompositeFramebuffers.resize(backBufferCount);
        }

        const auto& framebuffer = dm->GetCurrentSwapchainImage().Framebuffer;
        const nvrhi::FramebufferHandle framebufferHandle = framebuffer->GetFramebuffer();
        Ref<RenderPass>& renderPass = m_CompositePasses.at(backBufferIndex);

        // Lazy load pipeline since this is only used in the runtime
        if (!renderPass || m_CompositeFramebuffers.at(backBufferIndex) != framebufferHandle)
        {
            const PipelineSpecification pipelineSpec{
                .Shader = m_ShaderLibrary.Get("composite"),
                .Framebuffer = framebuffer,
                .Width = framebuffer->GetWidth(),
                .Height = framebuffer->GetHeight(),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            renderPass = CreateRef<RenderPass>(RenderPassSpecification{
                .Name = "Composite",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec),
            });

            m_CompositeFramebuffers.at(backBufferIndex) = framebufferHandle;
        }

        renderPass->SetInput(0, 0, image);
        renderPass->SetInput(0, 0, m_CompositeSampler);
        renderPass->Bake();

        m_CompositeCommandBuffer->Begin("Composite");
        BeginRenderPass(m_CompositeCommandBuffer, renderPass);

        m_CompositeCommandBuffer->GetCommandList()->draw(
            nvrhi::DrawArguments{
                .vertexCount = 3,
                .instanceCount = 1,
            }
        );

        EndRenderPass(m_CompositeCommandBuffer);
        m_CompositeCommandBuffer->End();
        m_CompositeCommandBuffer->Submit();
    }

    auto Renderer::GetShader(const std::string& name) const -> Ref<Shader>
    {
        return m_ShaderLibrary.Get(name);
    }

    auto Renderer::GetDescriptorManager() const -> const Ref<DescriptorManager>&
    {
        return m_DescriptorManager;
    }
}
