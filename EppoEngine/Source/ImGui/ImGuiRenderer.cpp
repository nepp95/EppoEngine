#include "pch.h"
#include "ImGui/ImGuiRenderer.h"

#include "Renderer/DescriptorManager.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"

// TODO: TEMPORARY
#include "Platform/Vulkan/Swapchain.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <nvrhi/utils.h>

namespace Eppo
{
    struct ImGuiViewportData
    {
        bool WindowOwned = false;
        ScopedPtr<Swapchain> Swapchain = nullptr;
        ScopedPtr<ImGuiRenderer> Renderer = nullptr;
    };

    ImGuiRenderer::ImGuiRenderer()
    {
        const auto& dm = DeviceManager::Get();
        const auto& renderer = dm->GetRenderer();
        const auto device = dm->GetDevice();

        auto& io = ImGui::GetIO();
        io.BackendRendererName = "ImGuiRenderer";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

        const auto& shader = renderer->GetShader("imgui");

        // Alpha blending for UI compositing. The template has no FramebufferInfo —
        // GetOrCreateRenderPass fills that in per swapchain.
        nvrhi::BlendState blendState;
        blendState.targets[0].blendEnable = true;
        blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
        blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::One;
        blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;

        m_PipelineSpecTemplate = PipelineSpecification{
            .Shader = shader,
            .CullMode = nvrhi::RasterCullMode::None,
            .FillMode = nvrhi::RasterFillMode::Solid,
            .DepthTestEnable = false,
            .DepthWriteEnable = true,
            .DepthFunc = nvrhi::ComparisonFunc::Always,
            .BlendState = blendState,
        };

        // We know we only have one binding layout
        const auto& bindingLayouts = shader->GetBindingLayouts();
        m_BindingSetLayout = bindingLayouts.at(0);

        // Sampler
        nvrhi::SamplerDesc samplerDesc{};
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
        samplerDesc.setAllFilters(true);

        m_FontSampler = device->createSampler(samplerDesc);
        EP_ASSERT(m_FontSampler);

        // Command list
        m_CommandList = device->createCommandList();
    }

    auto ImGuiRenderer::Resize() -> void
    {
        m_RenderPassCache.clear();
    }

    auto ImGuiRenderer::UpdateFontTexture() -> void
    {
        EP_PROFILE_FN("ImGuiRenderer::UpdateFontTexture")

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();
        auto& io = ImGui::GetIO();

        if (m_FontTexture && io.Fonts->TexRef.GetTexID())
            return;

        unsigned char* pixels;
        int width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        EP_ASSERT(pixels);

        nvrhi::TextureDesc textureDesc{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .format = nvrhi::Format::RGBA8_UNORM,
            .debugName = "ImGui Font Texture",
        };

        m_FontTexture = device->createTexture(textureDesc);
        EP_ASSERT(m_FontTexture);

        m_CommandList->open();
        m_CommandList->beginTrackingTextureState(m_FontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
        m_CommandList->writeTexture(m_FontTexture, 0, 0, pixels, static_cast<size_t>(width) * 4);
        m_CommandList->setPermanentTextureState(m_FontTexture, nvrhi::ResourceStates::ShaderResource);
        m_CommandList->commitBarriers();
        m_CommandList->close();

        device->executeCommandList(m_CommandList);
        io.Fonts->TexRef = ImTextureRef(m_FontTexture.Get());
    }

    auto ImGuiRenderer::RenderToSwapchain(ImGuiViewport* viewport, const ScopedPtr<Swapchain>& swapchain, const bool clearSwapchainTarget)
        -> void
    {
        EP_PROFILE_FN("ImGuiRenderer::RenderToSwapchain")

        Render(viewport, GetOrCreateRenderPass(swapchain), clearSwapchainTarget);
    }

    auto ImGuiRenderer::Render(ImGuiViewport* viewport, const Ref<RenderPass>& renderPass, const bool clearTarget) -> void
    {
        EP_PROFILE_FN("ImGuiRenderer::Render")

        m_Stats = {};
        const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();

        m_RenderCommandBuffer.Begin("UI");

        nvrhi::GraphicsState state{};

        const nvrhi::FramebufferHandle framebuffer = renderPass->GetFramebuffer()->GetFramebuffer();
        if (clearTarget)
            nvrhi::utils::ClearColorAttachment(m_RenderCommandBuffer.GetCommandList(), framebuffer, 0, nvrhi::Color(1, 0, 0, 1));

        // Update geometry
        ImDrawData* drawData = viewport->DrawData;
        UpdateGeometry(drawData);

        // DPI Scaling
        drawData->ScaleClipRects(drawData->FramebufferScale);

        // Setup graphics state
        struct PushConstants
        {
            glm::vec2 Scale;
            glm::vec2 Translate;
        } pushConstants{};

        pushConstants.Scale.x = 2.0f / drawData->DisplaySize.x;
        pushConstants.Scale.y = -2.0f / drawData->DisplaySize.y;
        pushConstants.Translate.x = -1.0f - drawData->DisplayPos.x * pushConstants.Scale.x;
        pushConstants.Translate.y = 1.0f - drawData->DisplayPos.y * pushConstants.Scale.y;

        float fbWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
        float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;

        state.pipeline = renderPass->GetPipeline()->GetPipeline();
        state.framebuffer = framebuffer;

        state.viewport.addViewport(nvrhi::Viewport(fbWidth, fbHeight));
        state.viewport.scissorRects.resize(1);

        // Vertex buffer
        nvrhi::VertexBufferBinding vtxBufBinding{
            .buffer = m_VertexBuffer,
            .slot = 0,
            .offset = 0,
        };

        state.addVertexBuffer(vtxBufBinding);

        // Index buffer
        state.indexBuffer.buffer = m_IndexBuffer;
        state.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
        state.indexBuffer.offset = 0;

        // Draw calls
        ImVec2 clipOffset = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;
        uint32_t vtxOffset = 0;
        uint32_t idxOffset = 0;

        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* drawList = drawData->CmdLists[n];

            for (int i = 0; i < drawList->CmdBuffer.Size; i++)
            {
                const ImDrawCmd* drawCmd = &drawList->CmdBuffer[i];

                if (drawCmd->UserCallback)
                {
                    drawCmd->UserCallback(drawList, drawCmd);
                }
                else
                {
                    const nvrhi::BindingSetVector bindingSets{
                        GetOrCreateBindingSet((nvrhi::ITexture*)drawCmd->TexRef.GetTexID()),
                        descriptorManager->GetResourceDT(),
                        descriptorManager->GetSamplerDT(),
                    };
                    state.bindings = bindingSets;
                    EP_ASSERT(state.bindings[0]);

                    ImVec2 clipMin((drawCmd->ClipRect.x - clipOffset.x) * clipScale.x, (drawCmd->ClipRect.y - clipOffset.y) * clipScale.y);
                    ImVec2 clipMax((drawCmd->ClipRect.z - clipOffset.x) * clipScale.x, (drawCmd->ClipRect.w - clipOffset.y) * clipScale.y);

                    if (clipMin.x < 0.0f)
                        clipMin.x = 0.0f;
                    if (clipMin.y < 0.0f)
                        clipMin.y = 0.0f;
                    if (clipMax.x > fbWidth)
                        clipMax.x = fbWidth;
                    if (clipMax.y > fbHeight)
                        clipMax.y = fbHeight;
                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    state.viewport.scissorRects[0] = nvrhi::Rect(
                        static_cast<int>(clipMin.x), static_cast<int>(clipMax.x), static_cast<int>(clipMin.y), static_cast<int>(clipMax.y)
                    );

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = drawCmd->ElemCount,
                        .startIndexLocation = drawCmd->IdxOffset + idxOffset,
                        .startVertexLocation = drawCmd->VtxOffset + vtxOffset,
                    };

                    m_RenderCommandBuffer.SetGraphicsState(state);
                    m_RenderCommandBuffer.CommitGraphicsState();
                    m_RenderCommandBuffer.GetCommandList()->setPushConstants(&pushConstants, sizeof(PushConstants));
                    m_RenderCommandBuffer.GetCommandList()->drawIndexed(drawArgs);

                    m_Stats.DrawCalls++;
                }
            }
            idxOffset += drawList->IdxBuffer.Size;
            vtxOffset += drawList->VtxBuffer.Size;
        }

        // Geometry submitted for this viewport (draw calls counted above).
        m_Stats.Vertices += static_cast<uint32_t>(drawData->TotalVtxCount);
        m_Stats.Indices += static_cast<uint32_t>(drawData->TotalIdxCount);

        m_RenderCommandBuffer.End();
        m_RenderCommandBuffer.Submit();
    }

    auto ImGuiRenderer::GetOwnGPUTime(const uint32_t frameIndex) const -> float
    {
        return m_RenderCommandBuffer.GetTimeMs(frameIndex);
    }

    auto ImGuiRenderer::GetGPUTime(const uint32_t frameIndex) const -> float
    {
        float totalTime = GetOwnGPUTime(frameIndex);

        for (const auto& platformIO = ImGui::GetPlatformIO(); const ImGuiViewport* viewport : platformIO.Viewports)
        {
            if (viewport == ImGui::GetMainViewport())
                continue;

            if (const auto* vd = static_cast<ImGuiViewportData*>(viewport->RendererUserData); vd && vd->Renderer)
                totalTime += vd->Renderer->GetOwnGPUTime(frameIndex);
        }

        return totalTime;
    }

    auto ImGuiRenderer::GetStats() const -> PassStatistics
    {
        PassStatistics total = GetOwnStats();

        for (const auto& platformIO = ImGui::GetPlatformIO(); const ImGuiViewport* viewport : platformIO.Viewports)
        {
            if (viewport == ImGui::GetMainViewport())
                continue;

            if (const auto* vd = static_cast<ImGuiViewportData*>(viewport->RendererUserData); vd && vd->Renderer)
                total += vd->Renderer->GetOwnStats();
        }

        return total;
    }

    auto ImGuiRenderer::UpdateGeometry(ImDrawData* drawData) -> void
    {
        EP_PROFILE_FN("ImGuiRenderer::UpdateGeometry")

        // Calculate buffer size + a small margin since we can often have the case we just need a little bit more (menu's, hover effects,
        // etc...)
        const uint64_t requiredVtxSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        const uint64_t reallocateVtxSize = UINT64_MAX - requiredVtxSize > 1024 ? requiredVtxSize + 1024 : UINT64_MAX;
        if (!m_VertexBuffer || m_VertexBuffer->getDesc().byteSize < requiredVtxSize)
            m_VertexBuffer = ReallocateBuffer(reallocateVtxSize, false);

        const uint64_t requiredIdxSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
        const uint64_t reallocateIdxSize = UINT64_MAX - requiredIdxSize > 1024 ? requiredIdxSize + 1024 : UINT64_MAX;
        if (!m_IndexBuffer || m_IndexBuffer->getDesc().byteSize < requiredIdxSize)
            m_IndexBuffer = ReallocateBuffer(reallocateIdxSize, true);

        // Update local data
        m_LocalVertexData.resize(drawData->TotalVtxCount);
        m_LocalIndexData.resize(drawData->TotalIdxCount);
        ImDrawVert* vtxDst = m_LocalVertexData.data();
        ImDrawIdx* idxDst = m_LocalIndexData.data();

        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* drawList = drawData->CmdLists[n];

            std::memcpy(vtxDst, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(idxDst, drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vtxDst += drawList->VtxBuffer.Size;
            idxDst += drawList->IdxBuffer.Size;
        }

        m_RenderCommandBuffer.GetCommandList()->writeBuffer(
            m_VertexBuffer, m_LocalVertexData.data(), m_LocalVertexData.size() * sizeof(ImDrawVert)
        );
        m_RenderCommandBuffer.GetCommandList()->writeBuffer(
            m_IndexBuffer, m_LocalIndexData.data(), m_LocalIndexData.size() * sizeof(ImDrawIdx)
        );
    }

    auto ImGuiRenderer::ReallocateBuffer(const uint64_t size, const bool indexBuffer) -> nvrhi::BufferHandle
    {
        EP_PROFILE_FN("ImGuiRenderer::ReallocateBuffer")

        const auto device = DeviceManager::Get()->GetDevice();

        const nvrhi::BufferDesc bufferDesc{
            .byteSize = size,
            .debugName = indexBuffer ? "ImGui IndexBuffer" : "ImGui VertexBuffer",
            .isVertexBuffer = !indexBuffer ? true : false,
            .isIndexBuffer = indexBuffer ? true : false,
            .initialState = indexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer,
            .keepInitialState = true,
        };

        return device->createBuffer(bufferDesc);
    }

    auto ImGuiRenderer::GetOrCreateRenderPass(const ScopedPtr<Swapchain>& swapchain) -> const Ref<RenderPass>&
    {
        EP_PROFILE_FN("ImGuiRenderer::GetOrCreateRenderPass")

        const auto& swapchainFramebuffer = swapchain->GetCurrentSwapchainImage().Framebuffer;
        Ref<RenderPass>& renderPass = m_RenderPassCache[swapchain.get()];

        // One pipeline is compatible with every backbuffer of this swapchain, so only the target framebuffer is swapped per frame.
        if (!renderPass)
        {
            renderPass = CreateRef<RenderPass>(RenderPassSpecification{
                .Name = "ImGui",
                .Pipeline = CreateRef<Pipeline>(m_PipelineSpecTemplate, swapchainFramebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = swapchainFramebuffer,
                .OwnsFramebuffer = false,
            });
        }
        else
        {
            renderPass->SetFramebuffer(swapchainFramebuffer);
        }

        return renderPass;
    }

    auto ImGuiRenderer::GetOrCreateBindingSet(const nvrhi::TextureHandle& texture) -> nvrhi::BindingSetHandle
    {
        EP_PROFILE_FN("ImGuiRenderer::GetOrCreateBindingSet")

        if (const auto it = m_BindingSetCache.find(texture); it != m_BindingSetCache.end())
            return it->second;

        const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();

        nvrhi::BindingSetDesc desc{};
        desc.bindings = { nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2), nvrhi::BindingSetItem::Texture_SRV(0, texture),
                          nvrhi::BindingSetItem::Sampler(0, m_FontSampler) };

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(desc, m_BindingSetLayout);
        EP_ASSERT(bindingSet);

        m_BindingSetCache[texture] = bindingSet;
        return bindingSet;
    }
}
