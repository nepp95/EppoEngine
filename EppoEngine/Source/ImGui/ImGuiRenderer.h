#pragma once

#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Swapchain.h"

#include <imgui.h>
#include <nvrhi/nvrhi.h>

#include <map>

namespace Eppo
{
    class ImGuiRenderer;

    struct ImGuiViewportData
    {
        bool FrameAcquired = false;
        Ref<Swapchain> Swapchain = nullptr;
        ScopedPtr<ImGuiRenderer> Renderer = nullptr;
    };

    class ImGuiRenderer
    {
    public:
        ImGuiRenderer();

        auto Resize() -> void;
        auto UpdateFontTexture() -> void;
        auto RenderToSwapchain(ImGuiViewport* viewport, const Ref<Swapchain>& swapchain, bool clearSwapchainTarget = true) -> void;
        auto Render(ImGuiViewport* viewport, const Ref<RenderPass>& renderPass, bool clearTarget = true) -> void;

        [[nodiscard]] auto GetGPUTime(uint32_t frameIndex) const -> float;
        [[nodiscard]] auto GetOwnGPUTime(uint32_t frameIndex) const -> float;
        [[nodiscard]] auto GetStats() const -> PassStatistics;
        [[nodiscard]] auto GetOwnStats() const -> const PassStatistics& { return m_Stats; }

    private:
        auto UpdateGeometry(ImDrawData* drawData) -> void;
        auto ReallocateBuffer(uint64_t size, bool indexBuffer) -> nvrhi::BufferHandle;
        auto GetOrCreateRenderPass(const Ref<Swapchain>& swapchain) -> const Ref<RenderPass>&;
        auto GetOrCreateBindingSet(const nvrhi::TextureHandle& texture) -> nvrhi::BindingSetHandle;

    private:
        nvrhi::CommandListHandle m_CommandList = nullptr;

        RenderCommandBuffer m_RenderCommandBuffer;
        PassStatistics m_Stats{};

        // Template spec (FramebufferInfo filled per swapchain in GetOrCreateRenderPass).
        PipelineSpecification m_PipelineSpecTemplate{};

        // One pass per swapchain: all backbuffers share one FramebufferInfo, so only the target framebuffer changes per frame.
        std::map<Swapchain*, Ref<RenderPass>> m_RenderPassCache;

        nvrhi::BindingLayoutHandle m_BindingSetLayout = nullptr;

        nvrhi::BufferHandle m_VertexBuffer = nullptr;
        nvrhi::BufferHandle m_IndexBuffer = nullptr;
        std::vector<ImDrawVert> m_LocalVertexData;
        std::vector<ImDrawIdx> m_LocalIndexData;

        nvrhi::TextureHandle m_FontTexture = nullptr;
        nvrhi::SamplerHandle m_FontSampler = nullptr;

        std::unordered_map<nvrhi::TextureHandle, nvrhi::BindingSetHandle> m_BindingSetCache;
    };
}
