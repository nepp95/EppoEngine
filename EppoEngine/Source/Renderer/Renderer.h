#pragma once

#include "Renderer/Image.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderCommandQueue.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Sampler.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
    class DescriptorManager;

    class Renderer
    {
    public:
        Renderer();

        auto Init() -> void;
        // With packed shaders the engine set is compiled from those and their includes; without them, from disk.
        auto LoadShaders(const std::map<std::string, std::string>& packed = {}, const std::map<std::string, std::string>& includes = {})
            -> void;

        static auto Submit(RenderCommand command) -> void;
        static auto ExecuteRenderCommands() -> void;

        static auto BeginRenderPass(const Ref<RenderCommandBuffer>& commandBuffer, const Ref<RenderPass>& renderPass) -> void;
        static auto EndRenderPass(const Ref<RenderCommandBuffer>& commandBuffer) -> void;
        auto CompositeToSwapchain(const Ref<Image>& image) -> void;

        [[nodiscard]] auto GetShader(const std::string& name) const -> Ref<Shader>;
        [[nodiscard]] auto GetSampler(const SamplerSpecification& specification) -> Ref<Sampler>;
        [[nodiscard]] auto GetAllShaders() const -> const std::unordered_map<std::string, Ref<Shader>>& { return m_ShaderLibrary.GetAll(); }
        [[nodiscard]] auto GetDescriptorManager() const -> const Ref<DescriptorManager>&;

    private:
        ShaderLibrary m_ShaderLibrary;
        Ref<DescriptorManager> m_DescriptorManager = nullptr;
        std::unordered_map<uint64_t, Ref<Sampler>> m_Samplers;
        std::mutex m_SamplerMutex;

        Ref<RenderCommandBuffer> m_CompositeCommandBuffer = nullptr;
        Ref<Sampler> m_CompositeSampler = nullptr;
        std::vector<Ref<RenderPass>> m_CompositePasses;
        std::vector<nvrhi::FramebufferHandle> m_CompositeFramebuffers;

        RenderCommandQueue m_RenderCommandQueue;
    };
}
