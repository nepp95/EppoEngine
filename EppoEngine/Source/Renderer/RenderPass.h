#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/Pipeline.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
    class Image;
    class Sampler;
    class StorageBuffer;
    class UniformBuffer;

    struct PassStatistics
    {
        uint32_t DrawCalls = 0;
        uint32_t Meshes = 0;
        uint32_t Submeshes = 0;
        uint32_t Instances = 0;
        uint32_t Vertices = 0;
        uint32_t Indices = 0;

        auto operator+=(const PassStatistics& other) -> PassStatistics&
        {
            DrawCalls += other.DrawCalls;
            Meshes += other.Meshes;
            Submeshes += other.Submeshes;
            Instances += other.Instances;
            Vertices += other.Vertices;
            Indices += other.Indices;
            return *this;
        }
    };

    struct RenderPassSpecification
    {
        std::string Name;
        Ref<Pipeline> Pipeline = nullptr;
        Ref<Framebuffer> Framebuffer = nullptr;
        bool OwnsFramebuffer = true;

        nvrhi::TextureSubresourceSet Subresources = nvrhi::TextureSubresourceSet(0, 1, 0, 1); // 1 miplevel default

        bool ClearColorOnLoad = false;
        glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

        bool ClearDepthOnLoad = false;
        float DepthClearValue = 1.0f;
        uint32_t StencilClearValue = 0;
    };

    class RenderPass
    {
    public:
        RenderPass() = default;
        explicit RenderPass(RenderPassSpecification spec);
        ~RenderPass() = default;

        RenderPass(const RenderPass&) = delete;
        auto operator=(const RenderPass&) -> RenderPass& = delete;
        RenderPass(RenderPass&&) noexcept = default;
        auto operator=(RenderPass&&) noexcept -> RenderPass& = default;

        auto Resize(uint32_t width, uint32_t height) const -> void;

        auto SetFramebuffer(const Ref<Framebuffer>& framebuffer) -> void;
        auto SetSubresources(const nvrhi::TextureSubresourceSet& subresources) -> void { m_Specification.Subresources = subresources; }

        [[nodiscard]] auto GetSpecification() const -> const RenderPassSpecification& { return m_Specification; }
        [[nodiscard]] auto GetPipeline() const -> const Ref<Pipeline>& { return m_Specification.Pipeline; }
        [[nodiscard]] auto GetFramebuffer() const -> const Ref<Framebuffer>& { return m_Specification.Framebuffer; }
        [[nodiscard]] auto GetSubresources() const -> const nvrhi::TextureSubresourceSet& { return m_Specification.Subresources; }
        [[nodiscard]] auto GetName() const -> const std::string& { return m_Specification.Name; }
        [[nodiscard]] auto GetStatistics() -> PassStatistics& { return m_Statistics; }
        [[nodiscard]] auto GetStatistics() const -> const PassStatistics& { return m_Statistics; }

        auto SetInput(uint32_t set, uint32_t binding, const Ref<Image>& resource) -> void;
        auto SetInput(uint32_t set, uint32_t binding, const Ref<Sampler>& resource) -> void;
        auto SetInput(uint32_t set, uint32_t binding, const Ref<StorageBuffer>& resource) -> void;
        auto SetInput(uint32_t set, uint32_t binding, const Ref<UniformBuffer>& resource) -> void;

        auto Invalidate() -> void;
        auto Bake() -> void;

        [[nodiscard]] auto GetBindingSets() const -> const nvrhi::BindingSetVector& { return m_BindingSets; }

    private:
        auto SetInputInternal(uint32_t set, uint32_t binding, nvrhi::ResourceType type, const Ref<void>& resource) -> void;
        [[nodiscard]] auto IsValid() const -> bool;

    private:
        RenderPassSpecification m_Specification;
        PassStatistics m_Statistics;

        struct BindingInput
        {
            uint32_t Binding = 0;
            nvrhi::ResourceType Type = nvrhi::ResourceType::None;
            Ref<void> Owner = nullptr;
        };

        std::unordered_map<uint32_t, std::vector<BindingInput>> m_Inputs;
        std::unordered_map<uint32_t, nvrhi::BindingSetDesc> m_BakedBindingSetDescs;
        bool m_Invalidated = true;

        std::vector<nvrhi::BindingSetHandle> m_OwnedBindingSets;
        nvrhi::BindingSetVector m_BindingSets;
    };
}
