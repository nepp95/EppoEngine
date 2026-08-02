#include "pch.h"
#include "Renderer/SceneRenderer.h"

#include "Core/Application.h"
#include "Project/Project.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/GpuProfiler.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Eppo
{
    namespace
    {
        constexpr uint32_t s_ShadowMapSize = 2048;
        constexpr float s_ShadowCascadeSplitLambda = 0.85f;
        constexpr float s_ShadowDepthPaddingScale = 0.25f;
        constexpr float s_ShadowDistance = 100.0f;
        constexpr float s_ShadowBias = 0.0015f;
        constexpr float s_ShadowNormalBias = 0.002f;

        constexpr uint32_t s_MaxBloomMipLevels = 6;

        constexpr uint32_t s_IblEnvironmentSize = 512;
        constexpr uint32_t s_IblIrradianceSize = 32;
        constexpr uint32_t s_IblPrefilterSize = 128;
        constexpr uint32_t s_IblPrefilterMipLevels = 5;
        constexpr uint32_t s_IblBrdfLutSize = 256;

        // Per-face inverse view-projection for the layered cube bake.
        auto GenerateFaceInverseViewProjection(const uint32_t face) -> glm::mat4
        {
            constexpr std::array directions{
                glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
            };

            constexpr std::array ups{
                glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
            };

            const glm::mat4 proj = glm::perspective(glm::half_pi<float>(), 1.0f, 0.1f, 10.0f);
            const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), directions.at(face), ups.at(face));

            return glm::inverse(proj * view);
        }

    }

    SceneRenderer::SceneRenderer(const Ref<Scene>& scene, const SceneRendererSpecification& specification)
        : m_Scene(scene), m_DebugRenderingEnabled(specification.EnableDebugRendering)
    {
        EP_PROFILE_FN("SceneRenderer::SceneRenderer")

        static_assert(sizeof(DrawData) == 80);
        static_assert(offsetof(DrawData, InstanceOffset) == 64);
        static_assert(offsetof(DrawData, MaterialIndex) == 68);
        static_assert(sizeof(MaterialData) == 80);
        static_assert(offsetof(MaterialData, EmissiveMapIndex) == 16);
        static_assert(offsetof(MaterialData, BaseColor) == 32);
        static_assert(offsetof(MaterialData, EmissiveFactor) == 48);
        static_assert(offsetof(MaterialData, Metallic) == 60);
        static_assert(offsetof(MaterialData, Roughness) == 64);

        const auto& dm = DeviceManager::Get();
        const auto& renderer = dm->GetRenderer();

        m_Width = specification.Width == 0 ? Application::Get().GetWindow()->GetWidth() : specification.Width;
        m_Height = specification.Height == 0 ? Application::Get().GetWindow()->GetHeight() : specification.Height;

        m_RenderCommandBuffer = CreateRef<RenderCommandBuffer>();

        // Create samplers
        m_ClampAllFiltersFalseSampler = Sampler::Create(
            SamplerSpecification{
                .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                .AllFilters = false,
            }
        );

        m_ClampAllFiltersTrueSampler = Sampler::Create(
            SamplerSpecification{
                .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
            }
        );

        m_WrapAllFiltersTrueSampler = Sampler::Create(
            SamplerSpecification{
                .AddressModeU = nvrhi::SamplerAddressMode::Wrap,
                .AddressModeV = nvrhi::SamplerAddressMode::Wrap,
                .AddressModeW = nvrhi::SamplerAddressMode::Wrap,
                .AllFilters = true,
            }
        );

        m_EquirectSampler = Sampler::Create(
            SamplerSpecification{
                .AddressModeU = nvrhi::SamplerAddressMode::Wrap,
                .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
            }
        );

        // Create render passes
        // Shadow Depth
        {
            const auto shadowMap = CreateRef<Image>(ImageSpecification{
                .ImageFormat = nvrhi::Format::D32,
                .Width = s_ShadowMapSize,
                .Height = s_ShadowMapSize,
                .ArraySize = s_ShadowCascadeCount,
                .IsRenderTarget = true,
                .DebugName = "Image Shadow Cascades",
            });

            const FramebufferSpecification framebufferSpec{
                .Width = s_ShadowMapSize,
                .Height = s_ShadowMapSize,
                .Attachments = { FramebufferTextureSpecification(shadowMap) },
                .DebugName = "Framebuffer Shadow Cascades",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            // The pass renders every cascade in one layered draw, so it targets all array slices.
            const nvrhi::TextureSubresourceSet cascadeSubresources(0, 1, 0, s_ShadowCascadeCount);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("shadowDepth"),
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Shadow Depth",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer(cascadeSubresources)->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .Subresources = cascadeSubresources,
                .ClearDepthOnLoad = true,
                .DepthClearValue = 1.0f,
            };

            m_ShadowDepthPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Ssao Prepass
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
                .DebugName = "Framebuffer SSAO Prepass",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("ssaoPrepass"),
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "SSAO Prepass",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f),
                .ClearDepthOnLoad = true,
                .DepthClearValue = 1.0f,
            };

            m_SsaoPrePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // SSAO Evaluation
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::R8_UNORM },
                .DebugName = "Framebuffer SSAO Evaluation",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("ssao"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "SSAO Evaluation",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f),
            };

            m_SsaoEvaluationPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // SSAO Horizontal Blur
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::R8_UNORM },
                .DebugName = "Framebuffer SSAO Horizontal Blur",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("ssaoBlur"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "SSAO Horizontal Blur",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f),
            };

            m_SsaoBlurHorizontalPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // SSAO Vertical Blur
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::R8_UNORM },
                .DebugName = "Framebuffer SSAO Vertical Blur",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("ssaoBlur"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "SSAO Vertical Blur",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f),
            };

            m_SsaoBlurVerticalPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Geometry
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA16_FLOAT, nvrhi::Format::D32 },
                .DebugName = "Framebuffer Geometry",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("geometry"),
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Geometry",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                .ClearDepthOnLoad = true,
            };

            m_GeometryPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Skybox
        {
            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("skybox"),
                .CullMode = nvrhi::RasterCullMode::None,
                .DepthTestEnable = true,
                .DepthWriteEnable = false,
                .DepthFunc = nvrhi::ComparisonFunc::LessOrEqual,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Skybox",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_GeometryPass->GetFramebuffer()->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_GeometryPass->GetFramebuffer(),
                .OwnsFramebuffer = false,
            };

            m_SkyPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Bloom Downsample
        {
            const FramebufferSpecification framebufferSpec{
                .Width = glm::max(1u, m_Width / 2u),
                .Height = glm::max(1u, m_Height / 2u),
                .Attachments = { { nvrhi::Format::RGBA16_FLOAT, s_MaxBloomMipLevels } },
                .DebugName = "Framebuffer Bloom Downsample",
            };

            m_BloomPyramidFramebuffer = CreateRef<Framebuffer>(framebufferSpec);
            m_BloomMipLevels = glm::min(s_MaxBloomMipLevels, m_BloomPyramidFramebuffer->GetFinalImage()->GetMipLevels());

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("bloomDownSample"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Bloom Downsample",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_BloomPyramidFramebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_BloomPyramidFramebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            };

            m_BloomDownSamplePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Bloom Upsample
        {
            nvrhi::BlendState blendState{};
            auto& target = blendState.targets[0];
            target.blendEnable = true;
            target.srcBlend = nvrhi::BlendFactor::One;
            target.destBlend = nvrhi::BlendFactor::One;
            target.blendOp = nvrhi::BlendOp::Add;
            target.srcBlendAlpha = nvrhi::BlendFactor::One;
            target.destBlendAlpha = nvrhi::BlendFactor::One;
            target.blendOpAlpha = nvrhi::BlendOp::Add;

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("bloomUpSample"),
                .CullMode = nvrhi::RasterCullMode::None,
                .BlendState = blendState,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Bloom Upsample",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_BloomPyramidFramebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_BloomPyramidFramebuffer,
            };

            m_BloomUpSamplePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Bloom Composite
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA16_FLOAT },
                .DebugName = "Framebuffer Bloom Composite",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("bloomComposite"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Bloom Composite",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            };

            m_BloomCompositePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Tonemap
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM },
                .DebugName = "Framebuffer Tonemap",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("tonemap"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Tonemap",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            };

            m_TonemapPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Wireframe
        {
            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("wireframe"),
                .CullMode = nvrhi::RasterCullMode::None,
                .FillMode = nvrhi::RasterFillMode::Wireframe,
                .DepthTestEnable = false,
                .DepthWriteEnable = false,
                .DepthBias = -1,
                .SlopeScaledDepthBias = -1.0f,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Wireframe",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_TonemapPass->GetFramebuffer()->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_TonemapPass->GetFramebuffer(),
                .OwnsFramebuffer = false,
            };

            m_WireframePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Uniform buffers
        m_ShadowDepthUB = CreateRef<UniformBuffer>(sizeof(ShadowDepthData), "UniformBuffer Shadow Depth");
        m_SsaoUB = CreateRef<UniformBuffer>(sizeof(SsaoData), "UniformBuffer Ssao");
        m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Camera");
        m_LightsUB = CreateRef<UniformBuffer>(sizeof(LightData), "UniformBuffer Lights");
        m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), "UniformBuffer Environment");

        m_InstanceTransformsSB = CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Instance Transforms");
        m_WireframeInstanceSB =
            CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Wireframe Instance Transforms");
        m_DrawDataSB = CreateRef<StorageBuffer>(sizeof(DrawData), sizeof(DrawData), "StorageBuffer Draw Data");
        m_MaterialDataSB = CreateRef<StorageBuffer>(sizeof(MaterialData), sizeof(MaterialData), "StorageBuffer Material Data");

        // Fill ssao data
        for (uint32_t i = 0; i < s_SsaoKernelSize; i++)
        {
            glm::vec3 sample = { Utils::GenerateRandomFloat(0.0f, 1.0f) * 2.0f - 1.0f, Utils::GenerateRandomFloat(0.0f, 1.0f) * 2.0f - 1.0f,
                                 Utils::GenerateRandomFloat(0.0f, 1.0f) };
            sample = glm::normalize(sample) * Utils::GenerateRandomFloat(0.0f, 1.0f);

            const float t = static_cast<float>(i) / static_cast<float>(s_SsaoKernelSize - 1);
            sample *= glm::mix(0.1f, 1.0f, t * t);
            m_SsaoData.Kernel[i] = glm::vec4(sample, 0.0f);
        }

        // Inputs retain their resources and resolve current GPU handles whenever a pass bakes.
        m_ShadowDepthPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_ShadowDepthPass->SetInput(0, 1, m_ShadowDepthUB);

        m_SsaoPrePass->SetInput(0, 0, m_InstanceTransformsSB);
        m_SsaoPrePass->SetInput(0, 1, m_DrawDataSB);
        m_SsaoPrePass->SetInput(0, 1, m_CameraUB);
        m_SsaoEvaluationPass->SetInput(0, 0, m_ClampAllFiltersTrueSampler);
        m_SsaoEvaluationPass->SetInput(0, 1, m_CameraUB);
        m_SsaoEvaluationPass->SetInput(0, 2, m_SsaoUB);
        m_SsaoBlurHorizontalPass->SetInput(0, 0, m_ClampAllFiltersTrueSampler);
        m_SsaoBlurHorizontalPass->SetInput(0, 1, m_CameraUB);
        m_SsaoBlurVerticalPass->SetInput(0, 0, m_ClampAllFiltersTrueSampler);
        m_SsaoBlurVerticalPass->SetInput(0, 1, m_CameraUB);

        m_GeometryPass->SetInput(0, 0, m_WrapAllFiltersTrueSampler);
        m_GeometryPass->SetInput(0, 1, m_ClampAllFiltersTrueSampler);
        m_GeometryPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_GeometryPass->SetInput(0, 1, m_DrawDataSB);
        m_GeometryPass->SetInput(0, 2, m_MaterialDataSB);
        m_GeometryPass->SetInput(0, 1, m_ShadowDepthUB);
        m_GeometryPass->SetInput(0, 2, m_CameraUB);
        m_GeometryPass->SetInput(0, 3, m_LightsUB);
        m_GeometryPass->SetInput(0, 4, m_EnvironmentUB);
        m_GeometryPass->SetInput(0, 5, m_SsaoUB);

        m_SkyPass->SetInput(0, 1, m_CameraUB);
        m_SkyPass->SetInput(0, 3, m_EnvironmentUB);

        m_WireframePass->SetInput(0, 0, m_WireframeInstanceSB);
        m_WireframePass->SetInput(0, 1, m_CameraUB);
        m_WireframePass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetDepthImage());

        m_TonemapPass->SetInput(0, 0, m_GeometryPass->GetFramebuffer()->GetFinalImage());
        m_TonemapPass->SetInput(0, 0, m_ClampAllFiltersTrueSampler);
    }

    auto SceneRenderer::RenderGui() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::RenderGui")

        const auto& app = Application::Get();
        if (!app.GetImGuiLayer())
            return;
        const auto& dm = DeviceManager::Get();
        const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
        EP_ASSERT(frameIndex < dm->GetBackBufferCount());

        const auto& imguiRenderer = app.GetImGuiLayer()->GetMainImGuiRenderer();

        struct PassResult
        {
            PassStatistics Stats{};
            float TimeMs = 0.0f;

            auto operator+=(const PassResult& rhs) -> PassResult&
            {
                Stats += rhs.Stats;
                TimeMs += rhs.TimeMs;
                return *this;
            }
        };

        struct Node
        {
            Ref<RenderPass> Pass = nullptr;
            std::string Label;
            std::vector<Node> Children;
        };

        auto Group = [](std::string label, std::vector<Node> children) -> Node
        {
            return {
                .Label = std::move(label),
                .Children = std::move(children),
            };
        };

        auto SampleNode = [frameIndex, cb = m_RenderCommandBuffer](const auto& self, const Node& node) -> PassResult
        {
            if (node.Pass)
                return { .Stats = node.Pass->GetStatistics(), .TimeMs = cb->GetTimeMs(node.Pass->GetName(), frameIndex) };

            PassResult total{};
            for (const auto& child : node.Children)
                total += self(self, child);
            return total;
        };

        // Draws a node (recursively) and returns its accumulated stats/time, expanded or not.
        auto DrawPassNode = [&SampleNode](const auto& self, const Node& node) -> PassResult
        {
            const PassResult result = SampleNode(SampleNode, node);
            const std::string& label = node.Pass ? node.Pass->GetName() : node.Label;

            if (ImGui::TreeNodeEx(label.c_str(), 0, "%s: %.2fms", label.c_str(), result.TimeMs))
            {
                if (node.Pass)
                {
                    const auto& stats = result.Stats;
                    ImGui::Text("Draw calls: %u", stats.DrawCalls);
                    ImGui::Text("Meshes: %u", stats.Meshes);
                    ImGui::Text("Submeshes: %u", stats.Submeshes);
                    ImGui::Text("Instances: %u", stats.Instances);
                    ImGui::Text("Vertices: %u", stats.Vertices);
                    ImGui::Text("Indices: %u", stats.Indices);
                }
                else
                {
                    for (const auto& child : node.Children)
                        self(self, child);
                }
                ImGui::TreePop();
            }

            return result;
        };

        const std::vector<Node> passTree{
            { .Pass = m_ShadowDepthPass },
            Group(
                "SSAO",
                {
                    { .Pass = m_SsaoPrePass },
                    { .Pass = m_SsaoEvaluationPass },
                    { .Pass = m_SsaoBlurHorizontalPass },
                    { .Pass = m_SsaoBlurVerticalPass },
                }
            ),
            { .Pass = m_GeometryPass },
            { .Pass = m_SkyPass },
            Group(
                "Bloom",
                {
                    { .Pass = m_BloomDownSamplePass },
                    { .Pass = m_BloomUpSamplePass },
                    { .Pass = m_BloomCompositePass },
                }
            ),
            { .Pass = m_TonemapPass },
            { .Pass = m_WireframePass },
        };

        ImGui::Begin("Scene Renderer");

        // Scene passes and their subtotal.
        ImGui::SeparatorText("Scene");

        PassResult result{};
        for (const auto& node : passTree)
            result += DrawPassNode(DrawPassNode, node);

        ImGui::Text("Scene total: %u draw calls, %.2fms", result.Stats.DrawCalls, result.TimeMs);

        // UI is tracked and reported separately from the scene.
        ImGui::SeparatorText("UI");
        const PassStatistics uiStats = imguiRenderer->GetStats();
        ImGui::Text("UI: %.2fms", imguiRenderer->GetGPUTime(frameIndex));
        ImGui::Text("Draw calls: %u", uiStats.DrawCalls);
        ImGui::Text("Vertices: %u", uiStats.Vertices);
        ImGui::Text("Indices: %u", uiStats.Indices);

        // Everything on screen: scene passes plus UI.
        const std::string totalLabel = std::format("Total: {:.2f}ms", result.TimeMs + imguiRenderer->GetGPUTime(frameIndex));
        ImGui::SeparatorText(totalLabel.c_str());
        ImGui::Text("Draw calls: %u", result.Stats.DrawCalls + uiStats.DrawCalls);
        ImGui::Text("Vertices: %u", result.Stats.Vertices + uiStats.Vertices);
        ImGui::Text("Indices: %u", result.Stats.Indices + uiStats.Indices);

        ImGui::End();
    }

    auto SceneRenderer::BeginScene(const EditorCamera& camera) -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginScene")

        m_CameraData.View = camera.GetViewMatrix();
        m_CameraData.Projection = camera.GetProjectionMatrix();
        m_CameraData.ViewProjection = camera.GetViewProjection();
        m_CameraData.Position = glm::vec4(camera.GetPosition(), 0.0f);
        m_CameraData.NearClip = camera.GetNearClip();
        m_CameraData.FarClip = camera.GetFarClip();

        BeginSceneInternal();
    }

    auto SceneRenderer::BeginScene(const SceneCamera& camera, const glm::mat4& transform) -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginScene")

        m_CameraData.View = glm::inverse(transform);
        m_CameraData.Projection = camera.GetProjectionMatrix();
        m_CameraData.ViewProjection = m_CameraData.Projection * m_CameraData.View;
        m_CameraData.Position = glm::vec4(glm::vec3(transform[3]), 0.0f);
        m_CameraData.NearClip = camera.GetPerspectiveNearClip();
        m_CameraData.FarClip = camera.GetPerspectiveFarClip();

        BeginSceneInternal();
    }

    auto SceneRenderer::EndScene() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EndScene")

        EnsureColliderMeshes();

        m_RenderCommandBuffer->Begin();
        PrepareRender();

        ShadowDepthPass();
        SsaoPass();
        GeometryPass();
        SkyPass();
        BloomPass();
        TonemapPass();
        WireframePass();

        // Collect resolves query results, which is illegal inside a render pass; every pass above closes its own.
        EP_GPU_COLLECT(m_RenderCommandBuffer);

        m_RenderCommandBuffer->End();
        m_RenderCommandBuffer->Submit();
    }

    auto SceneRenderer::GetFinalImage() const -> const Ref<Image>&
    {
        return m_TonemapPass->GetFramebuffer()->GetFinalImage();
    }

    auto SceneRenderer::SubmitMesh(const AssetHandle meshHandle, const glm::mat4& transform) -> void
    {
        const DrawKey key{
            .ID = meshHandle,
        };

        if (m_DrawCommands.contains(key))
        {
            auto& drawCmd = m_DrawCommands.at(key);
            drawCmd.Transforms.emplace_back(transform);
        }
        else
        {
            const auto& mesh = Project::GetActive()->GetAssetManager()->GetOrLoadAsset<Mesh>(meshHandle);

            const DrawCommand cmd{
                .Mesh = mesh,
                .Transforms = { transform },
            };

            m_DrawCommands[key] = cmd;
        }
    }

    auto SceneRenderer::SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, const float intensity) -> void
    {
        if (m_LightData.HasDirectionalLight)
            Log::Warn("Directional light was already set, overwriting");

        m_LightData.DirectionalLight = {
            .Direction = glm::vec4(direction, 0.0f),
            .Color = glm::vec4(color, intensity),
        };

        m_LightData.HasDirectionalLight = 1;
    }

    auto SceneRenderer::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, const float intensity) -> void
    {
        if (m_LightData.NumLights >= MaxPointLights)
        {
            Log::Warn("Scene has more than {} point lights; extra lights are ignored.", MaxPointLights);
            return;
        }

        auto& light = m_LightData.Lights.at(m_LightData.NumLights);
        light.Position = glm::vec4(position, 1.0f);
        light.Color = glm::vec4(color, intensity);

        m_LightData.NumLights++;
    }

    auto SceneRenderer::SubmitEnvironmentSettings(const EnvironmentSettings& environment) -> void
    {
        m_EnvironmentData.ZenithColor = glm::vec4(environment.ZenithColor, 1.0f);
        m_EnvironmentData.HorizonColor = glm::vec4(environment.HorizonColor, 1.0f);
        m_EnvironmentData.GroundColor = glm::vec4(environment.GroundColor, 1.0f);
        m_EnvironmentData.Params.x = environment.AmbientIntensity;

        // The environment cube remembers (in its Handle) the skybox it was baked from, so nothing else
        // tracks the baked state. Rebake only on change; Params.y and the IBL indices persist otherwise.
        if (const AssetHandle baked = m_EnvironmentCube ? m_EnvironmentCube->Handle : AssetHandle(0); environment.SkyboxHandle == baked)
            return;

        m_EnvironmentData.Params.y = 0.0f;
        m_EnvironmentData.IBL0 = glm::uvec4(0);
        m_EnvironmentData.IBL1 = glm::uvec4(0);

        // Gradient-only scenes never allocate the IBL targets; only clear a previously baked cube.
        if (!environment.SkyboxHandle)
        {
            if (m_EnvironmentCube)
                m_EnvironmentCube->Handle = 0;
            return;
        }

        // Allocate and record the attempt before loading, so a failed load doesn't retry every frame.
        EnsureIblResources();
        m_EnvironmentCube->Handle = environment.SkyboxHandle;

        // Image is an asset; resolves through the registry like meshes do.
        const auto& image = Project::GetActive()->GetAssetManager()->GetOrLoadAsset<Image>(environment.SkyboxHandle);
        if (!image)
        {
            Log::Error("Failed to load skybox image for handle {}", static_cast<uint64_t>(environment.SkyboxHandle));
            return;
        }

        BakeEnvironmentMap(image);

        m_EnvironmentData.Params.y = 1.0f;
        m_EnvironmentData.IBL0 = glm::uvec4(
            m_EnvironmentCube->GetBindlessIndex(), m_IrradianceCube->GetBindlessIndex(), m_PrefilterCube->GetBindlessIndex(),
            m_BrdfLut->GetBindlessIndex()
        );
        m_EnvironmentData.IBL1 = glm::uvec4(m_ClampAllFiltersTrueSampler->GetBindlessIndex(), 0, 0, 0);
    }

    auto SceneRenderer::SubmitBloomSettings(const BloomSettings& bloom) -> void
    {
        m_BloomSettings = bloom;
    }

    auto SceneRenderer::SubmitSsaoSettings(const SsaoSettings& ssao) -> void
    {
        m_SsaoSettings = ssao;
    }

    auto SceneRenderer::Resize(const uint32_t width, const uint32_t height) -> void
    {
        EP_PROFILE_FN("SceneRenderer::Resize")

        if (m_Width == width && m_Height == height)
            return;

        m_Width = width;
        m_Height = height;

        m_SsaoPrePass->Resize(m_Width, m_Height);
        m_SsaoEvaluationPass->Resize(m_Width, m_Height);
        m_SsaoBlurHorizontalPass->Resize(m_Width, m_Height);
        m_SsaoBlurVerticalPass->Resize(m_Width, m_Height);
        m_GeometryPass->Resize(m_Width, m_Height);
        m_SkyPass->Resize(m_Width, m_Height);
        m_BloomCompositePass->Resize(m_Width, m_Height);
        m_TonemapPass->Resize(m_Width, m_Height);
        m_WireframePass->Resize(m_Width, m_Height);

        m_BloomPyramidFramebuffer->Resize(glm::max(1u, m_Width / 2u), glm::max(1u, m_Height / 2u));
        m_BloomMipLevels = glm::min(s_MaxBloomMipLevels, m_BloomPyramidFramebuffer->GetFinalImage()->GetMipLevels());
    }

    auto SceneRenderer::BeginSceneInternal() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginSceneInternal")

        std::memset(&m_ShadowDepthPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SsaoPrePass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SsaoEvaluationPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SsaoBlurHorizontalPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SsaoBlurVerticalPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_GeometryPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SkyPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_BloomDownSamplePass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_BloomUpSamplePass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_BloomCompositePass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_WireframePass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_TonemapPass->GetStatistics(), 0, sizeof(PassStatistics));

        m_DrawCommands.clear();
        m_LightData.NumLights = 0;
        m_LightData.HasDirectionalLight = 0;

        m_CameraData.InverseViewProjection = glm::inverse(m_CameraData.ViewProjection);
    }

    auto SceneRenderer::EnsureColliderMeshes() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EnsureColliderMeshes")

        if (!m_DebugRenderingEnabled || (!m_ShowColliders && !m_HighlightedEntity))
            return;

        const auto& project = Project::GetActive();
        if (!project)
            return;

        const auto& assetManager = project->GetAssetManager();
        if (!m_BoxColliderMesh)
            m_BoxColliderMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cube));

        if (!m_SphereColliderMesh)
            m_SphereColliderMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Sphere));
        if (!m_CapsuleColliderMesh)
            m_CapsuleColliderMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Capsule));
        if (!m_CylinderColliderMesh)
            m_CylinderColliderMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cylinder));
    }

    auto SceneRenderer::GatherWireframes() -> void
    {
        EP_PROFILE_FN("SceneRenderer::GatherWireframes")

        m_WireframeDrawCommands.clear();
        if (!m_DebugRenderingEnabled)
            return;

        const auto& project = Project::GetActive();
        if (!project)
            return;

        constexpr auto colliderColor = glm::vec4(0.2f, 0.8f, 0.3f, 1.0f);
        constexpr auto highlightColor = glm::vec4(0.91f, 0.39f, 0.11f, 1.0f);
        constexpr auto meshWireframeColor = glm::vec4(0.45f, 0.63f, 0.95f, 1.0f);
        const auto& assetManager = project->GetAssetManager();

        if (m_ShowColliders)
        {
            DrawCommand boxDraw{
                .Mesh = m_BoxColliderMesh,
                .Color = colliderColor,
            };

            DrawCommand sphereDraw{
                .Mesh = m_SphereColliderMesh,
                .Color = colliderColor,
            };

            DrawCommand capsuleDraw{
                .Mesh = m_CapsuleColliderMesh,
                .Color = colliderColor,
            };

            DrawCommand cylinderDraw{
                .Mesh = m_CylinderColliderMesh,
                .Color = colliderColor,
            };

            m_Scene->ForEachEntity(
                [&](const Entity entity) -> void
                {
                    const glm::mat4 world = m_Scene->GetWorldTransform(entity);

                    if (entity.HasComponent<BoxColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<BoxColliderComponent>();
                        boxDraw.Transforms.emplace_back(glm::scale(glm::translate(world, c.Offset), c.HalfSize));
                    }

                    if (entity.HasComponent<SphereColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<SphereColliderComponent>();
                        sphereDraw.Transforms.emplace_back(glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius)));
                    }

                    if (entity.HasComponent<CapsuleColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<CapsuleColliderComponent>();
                        capsuleDraw.Transforms.emplace_back(
                            glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius, c.Height / 2.0f, c.Radius))
                        );
                    }

                    if (entity.HasComponent<CylinderColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<CylinderColliderComponent>();
                        cylinderDraw.Transforms.emplace_back(
                            glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius, c.Height / 2.0f, c.Radius))
                        );
                    }
                }
            );

            if (!boxDraw.Transforms.empty())
                m_WireframeDrawCommands.emplace_back(std::move(boxDraw));
            if (!sphereDraw.Transforms.empty())
                m_WireframeDrawCommands.emplace_back(std::move(sphereDraw));
            if (!capsuleDraw.Transforms.empty())
                m_WireframeDrawCommands.emplace_back(std::move(capsuleDraw));
            if (!cylinderDraw.Transforms.empty())
                m_WireframeDrawCommands.emplace_back(std::move(cylinderDraw));
        }

        if (m_ShowWireframes)
        {
            for (const auto& drawCmd : m_DrawCommands | std::views::values)
            {
                DrawCommand draw{
                    .Mesh = drawCmd.Mesh,
                    .Transforms = drawCmd.Transforms,
                    .Color = meshWireframeColor,
                };

                m_WireframeDrawCommands.emplace_back(std::move(draw));
            }
        }

        if (m_HighlightedEntity && m_HighlightedEntity.HasComponent<MeshComponent>())
        {
            if (const auto& mc = m_HighlightedEntity.GetComponent<MeshComponent>(); mc.MeshHandle)
            {
                const auto mesh = assetManager->GetOrLoadAsset<Mesh>(mc.MeshHandle);
                if (const auto& bounds = mesh->GetBounds(); bounds.IsValid())
                {
                    const glm::mat4 world = m_Scene->GetWorldTransform(m_HighlightedEntity);
                    const glm::mat4 boxTransform = glm::scale(glm::translate(world, bounds.GetCenter()), bounds.GetHalfExtent());

                    DrawCommand draw{
                        .Mesh = m_BoxColliderMesh,
                        .Transforms = { boxTransform },
                        .Color = highlightColor,
                    };

                    m_WireframeDrawCommands.emplace_back(std::move(draw));
                }
            }
        }
    }

    auto SceneRenderer::PrepareRender() -> void
    {
        EP_PROFILE_FN("SceneRenderer::PrepareRender")

        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

        // Shadow depth
        FillShadowData();
        m_ShadowDepthUB->SetData(cmdList, &m_ShadowDepthData, sizeof(m_ShadowDepthData));

        // Ssao data
        m_SsaoData.Params = glm::vec4(m_SsaoSettings.Radius, m_SsaoSettings.Bias, m_SsaoSettings.Power, m_SsaoSettings.Intensity);
        m_SsaoData.InvSize = glm::vec4(1.0f / m_Width, 1.0f / m_Height, 0.0f, 0.0f);
        m_SsaoUB->SetData(cmdList, &m_SsaoData, sizeof(m_SsaoData));

        // Camera, light and environment uniforms
        m_CameraUB->SetData(cmdList, &m_CameraData, sizeof(CameraData));
        m_LightsUB->SetData(cmdList, &m_LightData, sizeof(LightData));
        m_EnvironmentUB->SetData(cmdList, &m_EnvironmentData, sizeof(EnvironmentData));

        // Instance storage buffer
        std::vector<glm::mat4> instanceTransforms;
        for (auto& drawCmd : m_DrawCommands | std::views::values)
        {
            drawCmd.InstanceOffset = static_cast<uint32_t>(instanceTransforms.size());
            instanceTransforms.insert(instanceTransforms.end(), drawCmd.Transforms.begin(), drawCmd.Transforms.end());
        }

        const uint64_t requiredSize = instanceTransforms.size() * sizeof(glm::mat4);
        m_InstanceTransformsSB->SetData(cmdList, instanceTransforms.data(), requiredSize);

        // Wireframes
        GatherWireframes();

        std::vector<glm::mat4> wireframeTransforms;
        for (auto& draw : m_WireframeDrawCommands)
        {
            draw.InstanceOffset = static_cast<uint32_t>(wireframeTransforms.size());
            wireframeTransforms.insert(wireframeTransforms.end(), draw.Transforms.begin(), draw.Transforms.end());
        }

        const uint64_t wireframeSize = wireframeTransforms.size() * sizeof(glm::mat4);
        m_WireframeInstanceSB->SetData(cmdList, wireframeTransforms.data(), wireframeSize);

        // Prepare draw/material storage buffers
        m_DrawData.clear();
        m_MaterialData.clear();

        for (const auto& drawCmd : m_DrawCommands | std::views::values)
        {
            if (drawCmd.Transforms.empty())
                continue;

            for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
            {
                for (const auto& p : submesh.Primitives)
                {
                    const Ref<Material>& material = p.Material;
                    const uint32_t materialIndex = static_cast<uint32_t>(m_MaterialData.size());

                    m_DrawData.emplace_back(
                        DrawData{
                            .Transform = submesh.LocalTransform,
                            .InstanceOffset = drawCmd.InstanceOffset,
                            .MaterialIndex = materialIndex,
                        }
                    );

                    m_MaterialData.emplace_back(
                        MaterialData{
                            .DiffuseMapIndex = material->GetDiffuseMapIndex(),
                            .NormalMapIndex = material->GetNormalMapIndex(),
                            .RoughMetMapIndex = material->GetRoughMetMapIndex(),
                            .AOMapIndex = material->GetAOMapIndex(),
                            .EmissiveMapIndex = material->GetEmissiveMapIndex(),
                            .BaseColor = material->BaseColor,
                            .EmissiveFactor = material->EmissiveFactor,
                            .Metallic = material->Metallic,
                            .Roughness = material->Roughness,
                        }
                    );
                }
            }
        }

        m_DrawDataSB->SetData(cmdList, m_DrawData.data(), m_DrawData.size() * sizeof(DrawData));
        m_MaterialDataSB->SetData(cmdList, m_MaterialData.data(), m_MaterialData.size() * sizeof(MaterialData));

        // Framebuffer attachments are recreated on resize.
        const auto& prepassDepth = m_SsaoPrePass->GetFramebuffer()->GetDepthImage();
        const auto& prepassNormal = m_SsaoPrePass->GetFramebuffer()->GetFinalImage();

        m_SsaoEvaluationPass->SetInput(0, 0, prepassDepth);
        m_SsaoEvaluationPass->SetInput(0, 1, prepassNormal);
        m_SsaoBlurHorizontalPass->SetInput(0, 0, m_SsaoEvaluationPass->GetFramebuffer()->GetFinalImage());
        m_SsaoBlurHorizontalPass->SetInput(0, 1, prepassDepth);
        m_SsaoBlurVerticalPass->SetInput(0, 0, m_SsaoBlurHorizontalPass->GetFramebuffer()->GetFinalImage());
        m_SsaoBlurVerticalPass->SetInput(0, 1, prepassDepth);
        m_GeometryPass->SetInput(0, 3, m_SsaoBlurVerticalPass->GetFramebuffer()->GetFinalImage());
        m_TonemapPass->SetInput(0, 0, m_BloomCompositePass->GetFramebuffer()->GetFinalImage());
        m_WireframePass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetDepthImage());

        // Rebuild binding sets for any pass whose resource handles changed.
        m_ShadowDepthPass->Bake();
        m_SsaoPrePass->Bake();
        m_SsaoEvaluationPass->Bake();
        m_SsaoBlurHorizontalPass->Bake();
        m_SsaoBlurVerticalPass->Bake();
        m_GeometryPass->Bake();
        m_SkyPass->Bake();
        m_BloomDownSamplePass->Bake();
        m_BloomUpSamplePass->Bake();
        m_BloomCompositePass->Bake();
        m_TonemapPass->Bake();
        m_WireframePass->Bake();

        // Pre-register the subresources bloom samples bindlessly: GetBindlessIndex lazily writes the bindless
        // table on first use, which is illegal once an earlier pass has bound it, so warm the cache here first.
        static_cast<void>(m_GeometryPass->GetFramebuffer()->GetFinalImage()->GetBindlessIndex(nvrhi::TextureSubresourceSet(0, 1, 0, 1)));
        const auto& bloomPyramid = m_BloomPyramidFramebuffer->GetFinalImage();
        for (uint32_t mip = 0; mip < m_BloomMipLevels; mip++)
            static_cast<void>(bloomPyramid->GetBindlessIndex(nvrhi::TextureSubresourceSet(mip, 1, 0, 1)));
    }

    auto SceneRenderer::FillShadowData() -> void
    {
        EP_PROFILE_FN("SceneRenderer::FillShadowData")

        m_ShadowDepthData = {
            .DepthBias = s_ShadowBias,
            .NormalBias = s_ShadowNormalBias,
            .InvMapSize = 1.0f / s_ShadowMapSize,
            .ShadowDistance = s_ShadowDistance,
        };

        if (!m_LightData.HasDirectionalLight || m_DrawCommands.empty())
            return;

        auto lightDir = glm::vec3(m_LightData.DirectionalLight.Direction);
        const float directionLengthSq = glm::dot(lightDir, lightDir);
        if (directionLengthSq <= glm::epsilon<float>())
            return;
        lightDir /= glm::sqrt(directionLengthSq);

        const float nearClip = glm::max(m_CameraData.NearClip, 0.001f);
        const float farClip = glm::min(m_CameraData.FarClip, glm::max(s_ShadowDistance, nearClip));
        if (farClip <= nearClip)
            return;

        // View space depth distances
        std::array<float, s_ShadowCascadeCount> splits{};
        for (uint32_t i = 0; i < s_ShadowCascadeCount; i++)
        {
            const float fraction = static_cast<float>(i + 1) / static_cast<float>(s_ShadowCascadeCount);
            const float logarithmic = nearClip * glm::pow(farClip / nearClip, fraction);
            const float uniform = nearClip + (farClip - nearClip) * fraction;
            splits[i] = glm::mix(uniform, logarithmic, s_ShadowCascadeSplitLambda);
            m_ShadowDepthData.Cascades[i].SplitDistance = splits[i];
        }

        // Full-frustum world corners from the inverse view projection. The first four are the near
        // plane (NDC z = 0), the last four the far plane (NDC z = 1); Eppo uses zero-to-one depth.
        std::array<glm::vec3, 8> frustumCorners{};
        uint32_t cornerIndex = 0;
        for (uint32_t z = 0; z < 2; z++)
            for (uint32_t y = 0; y < 2; y++)
                for (uint32_t x = 0; x < 2; x++)
                {
                    const glm::vec4 ndc(static_cast<float>(x) * 2.0f - 1.0f, static_cast<float>(y) * 2.0f - 1.0f,
                        static_cast<float>(z), 1.0f);
                    const glm::vec4 world = m_CameraData.InverseViewProjection * ndc;
                    frustumCorners[cornerIndex++] = glm::vec3(world) / world.w;
                }

        constexpr glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 lightUp = glm::abs(glm::dot(lightDir, worldUp)) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : worldUp;

        float cascadeNear = nearClip;
        for (uint32_t cascade = 0; cascade < s_ShadowCascadeCount; cascade++)
        {
            const float cascadeFar = splits[cascade];
            const float nearRatio = glm::clamp((cascadeNear - nearClip) / (farClip - nearClip), 0.0f, 1.0f);
            const float farRatio = glm::clamp((cascadeFar - nearClip) / (farClip - nearClip), 0.0f, 1.0f);

            // Slice this cascade's eight corners along each near->far frustum edge.
            std::array<glm::vec3, 8> corners{};
            for (uint32_t i = 0; i < 4; i++)
            {
                const glm::vec3 edge = frustumCorners[i + 4] - frustumCorners[i];
                corners[i] = frustumCorners[i] + edge * nearRatio;
                corners[i + 4] = frustumCorners[i] + edge * farRatio;
            }

            glm::vec3 center(0.0f);
            for (const glm::vec3& corner : corners)
                center += corner;
            center /= static_cast<float>(corners.size());

            // Bounding-sphere radius, rounded up so small frustum changes don't resize the map.
            float radius = 0.0f;
            for (const glm::vec3& corner : corners)
                radius = glm::max(radius, glm::length(corner - center));
            radius = glm::ceil(radius * 16.0f) / 16.0f;

            // Snap the center to shadow texels so sub-texel camera motion doesn't shimmer. The snap
            // view shares its rotation with lightView below, so snapping here aligns the final grid.
            const float worldUnitsPerTexel = (2.0f * radius) / static_cast<float>(s_ShadowMapSize);
            const glm::mat4 snapView = glm::lookAt(-lightDir, glm::vec3(0.0f), lightUp);
            glm::vec4 snappedCenter = snapView * glm::vec4(center, 1.0f);
            snappedCenter.x = glm::floor(snappedCenter.x / worldUnitsPerTexel) * worldUnitsPerTexel;
            snappedCenter.y = glm::floor(snappedCenter.y / worldUnitsPerTexel) * worldUnitsPerTexel;
            center = glm::vec3(glm::inverse(snapView) * snappedCenter);

            // Same convention as the single-map path: light behind the sphere, square extent, positive ortho depth.
            const float depthPadding = glm::max(radius * s_ShadowDepthPaddingScale, 0.1f);
            const glm::vec3 lightPosition = center - lightDir * (radius + depthPadding);
            const glm::mat4 lightView = glm::lookAt(lightPosition, center, lightUp);
            const glm::mat4 lightProjection = glm::ortho(-radius, radius, -radius, radius, depthPadding, radius * 2.0f + depthPadding);
            m_ShadowDepthData.Cascades[cascade].LightViewProjection = lightProjection * lightView;

            cascadeNear = cascadeFar;
        }

        const auto& shadowMap = m_ShadowDepthPass->GetFramebuffer()->GetDepthImage();
        m_ShadowDepthData.ShadowMapIndex = shadowMap->GetBindlessIndex(nvrhi::TextureSubresourceSet(0, 1, 0, s_ShadowCascadeCount));
        m_ShadowDepthData.ShadowSamplerIndex = m_ClampAllFiltersFalseSampler->GetBindlessIndex();
    }

    auto SceneRenderer::ShadowDepthPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::ShadowDepthPass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "ShadowDepthPass")

        struct PC
        {
            glm::mat4 Transform;
            uint32_t InstanceOffset;
        } pushConstants{};

        auto& statistics = m_ShadowDepthPass->GetStatistics();
        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

        m_RenderCommandBuffer->BeginTimerQuery(m_ShadowDepthPass->GetName());
        m_RenderCommandBuffer->BeginMarker(m_ShadowDepthPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_ShadowDepthPass);

        auto& state = m_RenderCommandBuffer->GetGraphicsState();

        for (const auto& drawCmd : m_DrawCommands | std::views::values)
        {
            const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
            if (instanceCount == 0)
                continue;

            // Each object instance is drawn once per cascade; the vertex shader derives the cascade
            // and object index from SV_InstanceID and writes SV_RenderTargetArrayIndex.
            const uint32_t layeredInstanceCount = instanceCount * s_ShadowCascadeCount;

            for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
            {
                const nvrhi::VertexBufferBinding vtxBufBinding{
                    .buffer = submesh.VertexBuffer->GetBuffer(),
                    .slot = 0,
                    .offset = 0,
                };

                state.vertexBuffers.resize(1);
                state.vertexBuffers[0] = vtxBufBinding;
                state.indexBuffer.buffer = submesh.IndexBuffer->GetBuffer();
                state.indexBuffer.format = nvrhi::Format::R32_UINT;
                state.indexBuffer.offset = 0;
                m_RenderCommandBuffer->CommitGraphicsState();

                pushConstants.Transform = submesh.LocalTransform;
                pushConstants.InstanceOffset = drawCmd.InstanceOffset;

                for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                {
                    cmdList->setPushConstants(&pushConstants, sizeof(PC));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = layeredInstanceCount,
                        .startIndexLocation = firstIndex,
                        .startVertexLocation = firstVertex,
                    };

                    cmdList->drawIndexed(drawArgs);

                    statistics.DrawCalls++;
                    statistics.Vertices += static_cast<uint32_t>(vertexCount) * layeredInstanceCount;
                    statistics.Indices += static_cast<uint32_t>(indexCount) * layeredInstanceCount;
                }
                statistics.Submeshes++;
            }
            statistics.Instances += layeredInstanceCount;
            statistics.Meshes++;
        }

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndMarker();
        m_RenderCommandBuffer->EndTimerQuery(m_ShadowDepthPass->GetName());
    }

    auto SceneRenderer::SsaoPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::SsaoPass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "SsaoPass")

        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();
        m_RenderCommandBuffer->BeginMarker("SSAO");

        // Prepass
        {
            struct PC
            {
                uint32_t DrawIndex;
            } pushConstants{};

            auto& statistics = m_SsaoPrePass->GetStatistics();

            m_RenderCommandBuffer->BeginTimerQuery(m_SsaoPrePass->GetName());
            Renderer::BeginRenderPass(m_RenderCommandBuffer, m_SsaoPrePass);

            auto& state = m_RenderCommandBuffer->GetGraphicsState();

            uint32_t drawIndex = 0;
            for (const auto& drawCmd : m_DrawCommands | std::views::values)
            {
                const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
                if (instanceCount == 0)
                    continue;

                for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
                {
                    const nvrhi::VertexBufferBinding vtxBufBinding{
                        .buffer = submesh.VertexBuffer->GetBuffer(),
                        .slot = 0,
                        .offset = 0,
                    };

                    state.vertexBuffers.resize(1);
                    state.vertexBuffers[0] = vtxBufBinding;
                    state.indexBuffer.buffer = submesh.IndexBuffer->GetBuffer();
                    state.indexBuffer.format = nvrhi::Format::R32_UINT;
                    state.indexBuffer.offset = 0;
                    m_RenderCommandBuffer->CommitGraphicsState();

                    for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                    {
                        pushConstants.DrawIndex = drawIndex++;
                        cmdList->setPushConstants(&pushConstants, sizeof(PC));

                        nvrhi::DrawArguments drawArgs{
                            .vertexCount = static_cast<uint32_t>(indexCount),
                            .instanceCount = instanceCount,
                            .startIndexLocation = firstIndex,
                            .startVertexLocation = firstVertex,
                        };

                        cmdList->drawIndexed(drawArgs);

                        statistics.DrawCalls++;
                        statistics.Vertices += static_cast<uint32_t>(vertexCount) * instanceCount;
                        statistics.Indices += static_cast<uint32_t>(indexCount) * instanceCount;
                    }
                    statistics.Submeshes++;
                }
                statistics.Instances += instanceCount;
                statistics.Meshes++;
            }

            EP_ASSERT(drawIndex == m_DrawData.size());

            Renderer::EndRenderPass(m_RenderCommandBuffer);
            m_RenderCommandBuffer->EndTimerQuery(m_SsaoPrePass->GetName());
        }

        // Explicit transition
        const auto& prepassDepth = m_SsaoPrePass->GetFramebuffer()->GetDepthImage();
        const auto& prepassNormal = m_SsaoPrePass->GetFramebuffer()->GetFinalImage();
        cmdList->setTextureState(prepassDepth->GetTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        cmdList->setTextureState(prepassNormal->GetTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // Evaluation
        {
            m_RenderCommandBuffer->BeginTimerQuery(m_SsaoEvaluationPass->GetName());
            Renderer::BeginRenderPass(m_RenderCommandBuffer, m_SsaoEvaluationPass);

            constexpr nvrhi::DrawArguments drawArgs{
                .vertexCount = 3,
                .instanceCount = 1,
            };
            cmdList->draw(drawArgs);

            auto& statistics = m_SsaoEvaluationPass->GetStatistics();
            statistics.DrawCalls++;
            statistics.Vertices += drawArgs.vertexCount;

            Renderer::EndRenderPass(m_RenderCommandBuffer);
            m_RenderCommandBuffer->EndTimerQuery(m_SsaoEvaluationPass->GetName());
        }

        const auto& evaluationImage = m_SsaoEvaluationPass->GetFramebuffer()->GetFinalImage();
        cmdList->setTextureState(evaluationImage->GetTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        // Horizontal blur
        {
            struct PC
            {
                glm::vec2 Direction = glm::vec2(1.0f, 0.0f);
                glm::vec2 InvSize;
            } pushConstants{};
            pushConstants.InvSize = glm::vec2(1.0f / m_Width, 1.0f / m_Height);

            m_RenderCommandBuffer->BeginTimerQuery(m_SsaoBlurHorizontalPass->GetName());
            Renderer::BeginRenderPass(m_RenderCommandBuffer, m_SsaoBlurHorizontalPass);

            cmdList->setPushConstants(&pushConstants, sizeof(PC));

            constexpr nvrhi::DrawArguments drawArgs{
                .vertexCount = 3,
                .instanceCount = 1,
            };
            cmdList->draw(drawArgs);

            auto& statistics = m_SsaoBlurHorizontalPass->GetStatistics();
            statistics.DrawCalls++;
            statistics.Vertices += drawArgs.vertexCount;

            Renderer::EndRenderPass(m_RenderCommandBuffer);
            m_RenderCommandBuffer->EndTimerQuery(m_SsaoBlurHorizontalPass->GetName());
        }

        // Vertical blur
        {
            struct PC
            {
                glm::vec2 Direction = glm::vec2(0.0f, 1.0f);
                glm::vec2 InvSize;
            } pushConstants{};
            pushConstants.InvSize = glm::vec2(1.0f / m_Width, 1.0f / m_Height);

            m_RenderCommandBuffer->BeginTimerQuery(m_SsaoBlurVerticalPass->GetName());
            Renderer::BeginRenderPass(m_RenderCommandBuffer, m_SsaoBlurVerticalPass);

            cmdList->setPushConstants(&pushConstants, sizeof(PC));

            constexpr nvrhi::DrawArguments drawArgs{
                .vertexCount = 3,
                .instanceCount = 1,
            };
            cmdList->draw(drawArgs);

            auto& statistics = m_SsaoBlurVerticalPass->GetStatistics();
            statistics.DrawCalls++;
            statistics.Vertices += drawArgs.vertexCount;

            Renderer::EndRenderPass(m_RenderCommandBuffer);
            m_RenderCommandBuffer->EndTimerQuery(m_SsaoBlurVerticalPass->GetName());
        }

        m_RenderCommandBuffer->EndMarker();
    }

    auto SceneRenderer::GeometryPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::GeometryPass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "GeometryPass")

        struct PC
        {
            uint32_t DrawIndex;
        } pushConstants{};

        auto& statistics = m_GeometryPass->GetStatistics();
        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

        m_RenderCommandBuffer->BeginTimerQuery(m_GeometryPass->GetName());

        const auto& shadowMap = m_ShadowDepthPass->GetFramebuffer()->GetDepthImage();
        cmdList->setTextureState(shadowMap->GetTexture(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

        m_RenderCommandBuffer->BeginMarker(m_GeometryPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_GeometryPass);

        auto& state = m_RenderCommandBuffer->GetGraphicsState();

        uint32_t drawIndex = 0;
        for (const auto& drawCmd : m_DrawCommands | std::views::values)
        {
            const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
            if (instanceCount == 0)
                continue;

            for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
            {
                const nvrhi::VertexBufferBinding vtxBufBinding{
                    .buffer = submesh.VertexBuffer->GetBuffer(),
                    .slot = 0,
                    .offset = 0,
                };

                state.vertexBuffers.resize(1);
                state.vertexBuffers[0] = vtxBufBinding;
                state.indexBuffer.buffer = submesh.IndexBuffer->GetBuffer();
                state.indexBuffer.format = nvrhi::Format::R32_UINT;
                state.indexBuffer.offset = 0;
                m_RenderCommandBuffer->CommitGraphicsState();

                for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                {
                    pushConstants.DrawIndex = drawIndex++;
                    cmdList->setPushConstants(&pushConstants, sizeof(PC));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = instanceCount,
                        .startIndexLocation = firstIndex,
                        .startVertexLocation = firstVertex,
                    };

                    cmdList->drawIndexed(drawArgs);

                    statistics.DrawCalls++;
                    statistics.Vertices += static_cast<uint32_t>(vertexCount) * instanceCount;
                    statistics.Indices += static_cast<uint32_t>(indexCount) * instanceCount;
                }
                statistics.Submeshes++;
            }
            statistics.Instances += instanceCount;
            statistics.Meshes++;
        }

        EP_ASSERT(drawIndex == m_DrawData.size());

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndMarker();
        m_RenderCommandBuffer->EndTimerQuery(m_GeometryPass->GetName());
    }

    auto SceneRenderer::SkyPass() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::SkyPass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "SkyPass")

        auto& statistics = m_SkyPass->GetStatistics();
        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

        m_RenderCommandBuffer->BeginTimerQuery(m_SkyPass->GetName());
        m_RenderCommandBuffer->BeginMarker(m_SkyPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_SkyPass);

        constexpr nvrhi::DrawArguments drawArgs{
            .vertexCount = 3,
            .instanceCount = 1,
        };
        cmdList->draw(drawArgs);

        statistics.DrawCalls++;
        statistics.Vertices += drawArgs.vertexCount;

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndMarker();
        m_RenderCommandBuffer->EndTimerQuery(m_SkyPass->GetName());
    }

    auto SceneRenderer::BloomPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BloomPass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "BloomPass")

        struct PC
        {
            glm::vec4 Params;
            glm::uvec4 Indices;
        } pushConstants{};

        constexpr nvrhi::DrawArguments drawArgs{
            .vertexCount = 3,
            .instanceCount = 1,
        };

        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();
        const auto& sceneImage = m_GeometryPass->GetFramebuffer()->GetFinalImage();
        const auto& pyramid = m_BloomPyramidFramebuffer->GetFinalImage();
        const uint32_t samplerIndex = m_ClampAllFiltersTrueSampler->GetBindlessIndex();

        m_RenderCommandBuffer->BeginMarker("Bloom");

        // Downsample
        {
            // Push constants:
            // Params: inverse source size xy, threshold, knee
            // Indices: source image, sampler, apply threshold, unused
            m_RenderCommandBuffer->BeginTimerQuery(m_BloomDownSamplePass->GetName());
            for (uint32_t dest = 0; dest < m_BloomMipLevels; dest++)
            {
                const bool base = dest == 0;
                const Ref<Image>& source = base ? sceneImage : pyramid;
                const uint32_t sourceMip = base ? 0u : dest - 1u;
                const nvrhi::TextureSubresourceSet sourceSub(sourceMip, 1, 0, 1);

                cmdList->setTextureState(source->GetTexture(), sourceSub, nvrhi::ResourceStates::ShaderResource);
                m_BloomDownSamplePass->SetSubresources(nvrhi::TextureSubresourceSet(dest, 1, 0, 1));

                m_RenderCommandBuffer->BeginMarker(m_BloomDownSamplePass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_BloomDownSamplePass);

                pushConstants.Params = glm::vec4(
                    1.0f / source->GetMipWidth(sourceMip), 1.0f / source->GetMipHeight(sourceMip), m_BloomSettings.Threshold,
                    m_BloomSettings.Knee
                );
                pushConstants.Indices = glm::uvec4(source->GetBindlessIndex(sourceSub), samplerIndex, base ? 1u : 0u, 0u);

                cmdList->setPushConstants(&pushConstants, sizeof(PC));
                cmdList->draw(drawArgs);

                auto& statistics = m_BloomDownSamplePass->GetStatistics();
                statistics.DrawCalls++;
                statistics.Vertices += drawArgs.vertexCount;

                Renderer::EndRenderPass(m_RenderCommandBuffer);
                m_RenderCommandBuffer->EndMarker();
            }
            m_RenderCommandBuffer->EndTimerQuery(m_BloomDownSamplePass->GetName());
        }

        // Upsample
        {
            // Push constants
            // Params: inverse source size xy, radius, unused
            // Indices: source image, sampler, unused, unused
            m_RenderCommandBuffer->BeginTimerQuery(m_BloomUpSamplePass->GetName());
            for (uint32_t sourceMip = m_BloomMipLevels - 1; sourceMip > 0; sourceMip--)
            {
                const uint32_t dest = sourceMip - 1;
                const nvrhi::TextureSubresourceSet sourceSub(sourceMip, 1, 0, 1);

                cmdList->setTextureState(pyramid->GetTexture(), sourceSub, nvrhi::ResourceStates::ShaderResource);
                m_BloomUpSamplePass->SetSubresources(nvrhi::TextureSubresourceSet(dest, 1, 0, 1));

                m_RenderCommandBuffer->BeginMarker(m_BloomUpSamplePass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_BloomUpSamplePass);

                pushConstants.Params = glm::vec4(
                    1.0f / pyramid->GetMipWidth(sourceMip), 1.0f / pyramid->GetMipHeight(sourceMip), m_BloomSettings.Radius, 0.0f
                );
                pushConstants.Indices = glm::uvec4(pyramid->GetBindlessIndex(sourceSub), samplerIndex, 0u, 0u);

                cmdList->setPushConstants(&pushConstants, sizeof(PC));
                cmdList->draw(drawArgs);

                auto& statistics = m_BloomUpSamplePass->GetStatistics();
                statistics.DrawCalls++;
                statistics.Vertices += drawArgs.vertexCount;

                Renderer::EndRenderPass(m_RenderCommandBuffer);
                m_RenderCommandBuffer->EndMarker();
            }
            m_RenderCommandBuffer->EndTimerQuery(m_BloomUpSamplePass->GetName());
        }

        // Composite
        {
            // Push constants
            // Params: intensity, unused, unused, unused
            // Indices: scene image, bloom image, sampler, unused
            const nvrhi::TextureSubresourceSet mipZero(0, 1, 0, 1);

            m_RenderCommandBuffer->BeginTimerQuery(m_BloomCompositePass->GetName());

            cmdList->setTextureState(sceneImage->GetTexture(), mipZero, nvrhi::ResourceStates::ShaderResource);
            cmdList->setTextureState(pyramid->GetTexture(), mipZero, nvrhi::ResourceStates::ShaderResource);

            m_RenderCommandBuffer->BeginMarker(m_BloomCompositePass->GetName());
            Renderer::BeginRenderPass(m_RenderCommandBuffer, m_BloomCompositePass);

            pushConstants.Params = glm::vec4(m_BloomSettings.Intensity, 0.0f, 0.0f, 0.0f);
            pushConstants.Indices = glm::uvec4(sceneImage->GetBindlessIndex(mipZero), pyramid->GetBindlessIndex(mipZero), samplerIndex, 0u);

            cmdList->setPushConstants(&pushConstants, sizeof(PC));
            cmdList->draw(drawArgs);

            auto& statistics = m_BloomCompositePass->GetStatistics();
            statistics.DrawCalls++;
            statistics.Vertices += drawArgs.vertexCount;

            Renderer::EndRenderPass(m_RenderCommandBuffer);
            m_RenderCommandBuffer->EndMarker();
            m_RenderCommandBuffer->EndTimerQuery(m_BloomCompositePass->GetName());
        }

        m_RenderCommandBuffer->EndMarker();
    }

    auto SceneRenderer::TonemapPass() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::TonemapPass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "TonemapPass")

        constexpr struct PC
        {
            float Exposure = 1.0f;
        } pushConstants{};

        auto& statistics = m_TonemapPass->GetStatistics();
        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

        m_RenderCommandBuffer->BeginTimerQuery(m_TonemapPass->GetName());
        m_RenderCommandBuffer->BeginMarker(m_TonemapPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_TonemapPass);
        cmdList->setPushConstants(&pushConstants, sizeof(PC));

        constexpr nvrhi::DrawArguments drawArgs{
            .vertexCount = 3,
            .instanceCount = 1,
        };
        cmdList->draw(drawArgs);

        statistics.DrawCalls++;
        statistics.Vertices += drawArgs.vertexCount;

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndMarker();
        m_RenderCommandBuffer->EndTimerQuery(m_TonemapPass->GetName());
    }

    auto SceneRenderer::WireframePass() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::WireframePass")
        EP_GPU_ZONE(m_RenderCommandBuffer, "WireframePass")

        if (!m_DebugRenderingEnabled || m_WireframeDrawCommands.empty())
        {
            m_RenderCommandBuffer->BeginTimerQuery(m_WireframePass->GetName());
            m_RenderCommandBuffer->EndTimerQuery(m_WireframePass->GetName());
            return;
        }

        struct PC
        {
            glm::mat4 Transform;
            glm::vec4 Color;
            uint32_t InstanceOffset;
        } pushConstants{};

        auto& statistics = m_WireframePass->GetStatistics();
        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

        m_RenderCommandBuffer->BeginTimerQuery(m_WireframePass->GetName());
        m_RenderCommandBuffer->BeginMarker(m_WireframePass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_WireframePass);

        auto& state = m_RenderCommandBuffer->GetGraphicsState();

        for (const auto& drawCmd : m_WireframeDrawCommands)
        {
            const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
            if (instanceCount == 0)
                continue;

            for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
            {
                const nvrhi::VertexBufferBinding vtxBufBinding{
                    .buffer = submesh.VertexBuffer->GetBuffer(),
                    .slot = 0,
                    .offset = 0,
                };

                state.vertexBuffers.resize(1);
                state.vertexBuffers[0] = vtxBufBinding;
                state.indexBuffer.buffer = submesh.IndexBuffer->GetBuffer();
                state.indexBuffer.format = nvrhi::Format::R32_UINT;
                state.indexBuffer.offset = 0;
                m_RenderCommandBuffer->CommitGraphicsState();

                pushConstants.Transform = submesh.LocalTransform;
                pushConstants.Color = drawCmd.Color;
                pushConstants.InstanceOffset = drawCmd.InstanceOffset;

                for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                {
                    cmdList->setPushConstants(&pushConstants, sizeof(PC));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = instanceCount,
                        .startIndexLocation = firstIndex,
                        .startVertexLocation = firstVertex,
                    };

                    cmdList->drawIndexed(drawArgs);

                    statistics.DrawCalls++;
                    statistics.Vertices += static_cast<uint32_t>(vertexCount);
                    statistics.Indices += static_cast<uint32_t>(indexCount);
                }
                statistics.Submeshes++;
            }
            statistics.Meshes++;
            statistics.Instances += instanceCount;
        }

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndMarker();
        m_RenderCommandBuffer->EndTimerQuery(m_WireframePass->GetName());
    }

    auto SceneRenderer::EnsureIblResources() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EnsureIblResources")

        if (m_EnvironmentCube)
            return;

        const auto& renderer = DeviceManager::Get()->GetRenderer();

        m_EnvironmentCube = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = s_IblEnvironmentSize,
            .Height = s_IblEnvironmentSize,
            .MipLevels = Image::CalculateMipLevels(s_IblEnvironmentSize, s_IblEnvironmentSize),
            .IsCubemap = true,
            .IsRenderTarget = true,
            .DebugName = "IBL Environment Cube",
        });

        m_IrradianceCube = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = s_IblIrradianceSize,
            .Height = s_IblIrradianceSize,
            .IsCubemap = true,
            .IsRenderTarget = true,
            .DebugName = "IBL Irradiance Cube",
        });

        m_PrefilterCube = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
            .Width = s_IblPrefilterSize,
            .Height = s_IblPrefilterSize,
            .MipLevels = s_IblPrefilterMipLevels,
            .IsCubemap = true,
            .IsRenderTarget = true,
            .DebugName = "IBL Prefilter Cube",
        });

        m_BrdfLut = CreateRef<Image>(ImageSpecification{
            .ImageFormat = nvrhi::Format::RG16_FLOAT,
            .Width = s_IblBrdfLutSize,
            .Height = s_IblBrdfLutSize,
            .IsRenderTarget = true,
            .DebugName = "IBL BRDF LUT",
        });

        // Bake the view-independent BRDF LUT once: a 2D fullscreen pass, no source/faces, so not RecordIblPass.
        const auto framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = s_IblBrdfLutSize,
            .Height = s_IblBrdfLutSize,
            .Attachments = { FramebufferTextureSpecification(m_BrdfLut) },
            .DebugName = "IBL BRDF LUT Framebuffer",
        });

        const auto pipeline = CreateRef<Pipeline>(
            PipelineSpecification{
                .Shader = renderer->GetShader("iblBrdfLut"),
                .CullMode = nvrhi::RasterCullMode::None,
            },
            framebuffer->GetFramebuffer()->getFramebufferInfo()
        );

        const auto pass = CreateRef<RenderPass>(RenderPassSpecification{
            .Name = "IBL BRDF LUT",
            .Pipeline = pipeline,
            .Framebuffer = framebuffer,
            .OwnsFramebuffer = false,
        });
        pass->Bake();

        const auto cmdBuffer = CreateRef<RenderCommandBuffer>();
        cmdBuffer->Begin();
        cmdBuffer->BeginMarker(pass->GetName());
        Renderer::BeginRenderPass(cmdBuffer, pass);

        constexpr nvrhi::DrawArguments drawArgs{ .vertexCount = 3, .instanceCount = 1 };
        cmdBuffer->GetCommandList()->draw(drawArgs);

        Renderer::EndRenderPass(cmdBuffer);
        cmdBuffer->EndMarker();
        cmdBuffer->End();
        cmdBuffer->Submit();
        DeviceManager::Get()->GetDevice()->waitForIdle();
    }

    auto SceneRenderer::BakeEnvironmentMap(const Ref<Image>& equirect) -> void
    {
        EP_PROFILE_FN("SceneRenderer::BakeEnvironmentMap")

        EnsureIblResources();

        const auto& renderer = DeviceManager::Get()->GetRenderer();

        // Local face-matrix UB (only touched while baking); the cube VS indexes it by SV_InstanceID.
        glm::mat4 inverseViewProjection[6];

        for (uint32_t face = 0; face < 6; face++)
            inverseViewProjection[face] = GenerateFaceInverseViewProjection(face);

        const auto facesUB = CreateRef<UniformBuffer>(sizeof(glm::mat4) * 6, "UniformBuffer IBL Faces");

        DeviceManager::Get()->GetDevice()->waitForIdle();
        for (uint32_t mip = 0; mip + 1 < m_EnvironmentCube->GetMipLevels(); mip++)
            static_cast<void>(m_EnvironmentCube->GetBindlessIndex(nvrhi::TextureSubresourceSet(mip, 1, 0, 6)));

        const auto cmdBuffer = CreateRef<RenderCommandBuffer>();
        cmdBuffer->Begin();

        facesUB->SetData(cmdBuffer->GetCommandList(), &inverseViewProjection, sizeof(glm::mat4) * 6);

        // Equirect -> environment cube. Wrap sampler so the atan2 longitude seam wraps cleanly.
        RecordIblPass(renderer->GetShader("iblEquirectToCube"), equirect, m_EquirectSampler, m_EnvironmentCube, facesUB, cmdBuffer, 0);

        for (uint32_t mip = 1; mip < m_EnvironmentCube->GetMipLevels(); mip++)
            RecordEnvironmentMipPass(m_EnvironmentCube, facesUB, cmdBuffer, mip);

        // Environment cube -> irradiance. Clamp for the cube convolution.
        RecordIblPass(
            renderer->GetShader("iblIrradiance"), m_EnvironmentCube, m_ClampAllFiltersTrueSampler, m_IrradianceCube, facesUB, cmdBuffer, 0,
            0.0f, static_cast<float>(s_IblEnvironmentSize)
        );

        // Environment cube -> prefiltered specular, one mip per roughness. Clamp.
        for (uint32_t mip = 0; mip < s_IblPrefilterMipLevels; mip++)
        {
            const float roughness = static_cast<float>(mip) / static_cast<float>(s_IblPrefilterMipLevels - 1);
            RecordIblPass(
                renderer->GetShader("iblPrefilter"), m_EnvironmentCube, m_ClampAllFiltersTrueSampler, m_PrefilterCube, facesUB, cmdBuffer,
                mip, roughness, static_cast<float>(s_IblEnvironmentSize)
            );
        }

        cmdBuffer->End();
        cmdBuffer->Submit();
        DeviceManager::Get()->GetDevice()->waitForIdle();
    }

    auto SceneRenderer::RecordEnvironmentMipPass(
        const Ref<Image>& target, const Ref<UniformBuffer>& facesUB, const Ref<RenderCommandBuffer>& cmdBuffer, const uint32_t mipLevel
    ) const -> void
    {
        EP_ASSERT(mipLevel > 0 && mipLevel < target->GetMipLevels());

        const auto shader = DeviceManager::Get()->GetRenderer()->GetShader("iblEnvironmentMip");

        const auto framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = target->GetWidth(),
            .Height = target->GetHeight(),
            .Attachments = { FramebufferTextureSpecification(target) },
            .DebugName = "IBL Environment Mip Framebuffer",
        });

        const auto pipeline = CreateRef<Pipeline>(
            PipelineSpecification{
                .Shader = shader,
                .CullMode = nvrhi::RasterCullMode::None,
            },
            framebuffer->GetFramebuffer()->getFramebufferInfo()
        );

        const auto pass = CreateRef<RenderPass>(RenderPassSpecification{
            .Name = "IBL Environment Mip",
            .Pipeline = pipeline,
            .Framebuffer = framebuffer,
            .OwnsFramebuffer = false,
            .Subresources = nvrhi::TextureSubresourceSet(mipLevel, 1, 0, 6),
        });

        pass->SetInput(0, 1, facesUB);
        pass->Bake();

        const nvrhi::TextureSubresourceSet sourceSubresources(mipLevel - 1, 1, 0, 6);
        const struct PC
        {
            uint32_t SourceIndex;
            uint32_t SamplerIndex;
        } pushConstants{
            .SourceIndex = target->GetBindlessIndex(sourceSubresources),
            .SamplerIndex = m_ClampAllFiltersTrueSampler->GetBindlessIndex(),
        };

        const auto& cmdList = cmdBuffer->GetCommandList();
        cmdList->setTextureState(target->GetTexture(), sourceSubresources, nvrhi::ResourceStates::ShaderResource);
        cmdBuffer->BeginMarker(pass->GetName());
        Renderer::BeginRenderPass(cmdBuffer, pass);
        cmdList->setPushConstants(&pushConstants, sizeof(PC));

        constexpr nvrhi::DrawArguments drawArgs{ .vertexCount = 3, .instanceCount = 6 };
        cmdList->draw(drawArgs);

        Renderer::EndRenderPass(cmdBuffer);
        cmdBuffer->EndMarker();
    }

    auto SceneRenderer::RecordIblPass(
        const Ref<Shader>& shader, const Ref<Image>& source, const Ref<Sampler>& sampler, const Ref<Image>& target,
        const Ref<UniformBuffer>& facesUB, const Ref<RenderCommandBuffer>& cmdBuffer, const uint32_t mipLevel, const float roughness,
        const float envMapSize
    ) -> void
    {
        const auto framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = target->GetWidth(),
            .Height = target->GetHeight(),
            .Attachments = { FramebufferTextureSpecification(target) },
            .DebugName = "IBL Bake Framebuffer",
        });

        // FramebufferInfo carries only formats/samples, so mip zero's handle describes every mip; the pass selects the mip below.
        const auto pipeline = CreateRef<Pipeline>(
            PipelineSpecification{
                .Shader = shader,
                .CullMode = nvrhi::RasterCullMode::None,
            },
            framebuffer->GetFramebuffer()->getFramebufferInfo()
        );

        const auto pass = CreateRef<RenderPass>(RenderPassSpecification{
            .Name = "IBL Bake",
            .Pipeline = pipeline,
            .Framebuffer = framebuffer,
            .OwnsFramebuffer = false,
            .Subresources = nvrhi::TextureSubresourceSet(mipLevel, 1, 0, 6),
        });

        if (source)
            pass->SetInput(0, 0, source);
        pass->SetInput(0, 0, sampler);
        pass->SetInput(0, 1, facesUB);
        pass->Bake();

        const struct PC
        {
            float Roughness = roughness;
            float EnvMapSize = envMapSize;
        } pushConstants{};

        const auto& cmdList = cmdBuffer->GetCommandList();
        cmdBuffer->BeginMarker(pass->GetName());
        Renderer::BeginRenderPass(cmdBuffer, pass);

        // Only the prefilter shader uses the push block; dxc strips it from the others, leaving no range.
        if (shader->GetPushConstants().Size > 0)
            cmdList->setPushConstants(&pushConstants, sizeof(PC));

        constexpr nvrhi::DrawArguments drawArgs{ .vertexCount = 3, .instanceCount = 6 };
        cmdList->draw(drawArgs);

        Renderer::EndRenderPass(cmdBuffer);
        cmdBuffer->EndMarker();
    }
}
