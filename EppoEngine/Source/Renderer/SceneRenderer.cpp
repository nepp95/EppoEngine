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
        constexpr float s_ShadowBiasTexels = 2.0f;
        constexpr float s_ShadowNormalBiasTexels = 1.0f;

        constexpr uint32_t s_PointShadowMapSize = 512;
        constexpr float s_PointShadowNearClip = 0.05f;

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

        const auto& dm = DeviceManager::Get();
        const auto& renderer = dm->GetRenderer();

        m_Width = specification.Width == 0 ? Application::Get().GetWindow()->GetWidth() : specification.Width;
        m_Height = specification.Height == 0 ? Application::Get().GetWindow()->GetHeight() : specification.Height;

        m_RenderCommandBuffer = CreateRef<RenderCommandBuffer>();

        // Create render passes
        // Shadow Depth
        {
            const auto shadowMap = Image::Create(
                ImageSpecification{
                    .ImageFormat = nvrhi::Format::D32,
                    .Width = s_ShadowMapSize,
                    .Height = s_ShadowMapSize,
                    .ArraySize = s_ShadowCascadeCount,
                    .IsRenderTarget = true,
                    .DebugName = "Image Shadow Cascades",
                }
            );

            const FramebufferSpecification framebufferSpec{
                .Width = s_ShadowMapSize,
                .Height = s_ShadowMapSize,
                .Attachments = { FramebufferTextureSpecification(shadowMap) },
                .DebugName = "Framebuffer Shadow Cascades",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            // The pass renders every cascade in one layered draw, so it targets all array slices.
            const nvrhi::TextureSubresourceSet cascadeSubresources(0, 1, 0, s_ShadowCascadeCount);

            PipelineSpecification pipelineSpec{
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

            // Shadow Depth Double Sided
            pipelineSpec.CullMode = nvrhi::RasterCullMode::None;
            m_ShadowDepthDoubleSidedPipeline =
                CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer(cascadeSubresources)->getFramebufferInfo());
        }

        // Point Shadow Depth
        {
            const auto shadowMap = Image::Create(
                ImageSpecification{
                    .ImageFormat = nvrhi::Format::D32,
                    .Width = s_PointShadowMapSize,
                    .Height = s_PointShadowMapSize,
                    .ArraySize = MaxPointLights * 6,
                    .IsCubemap = true,
                    .IsRenderTarget = true,
                    .DebugName = "Image Point Shadow Cubes",
                }
            );

            const FramebufferSpecification framebufferSpec{
                .Width = s_PointShadowMapSize,
                .Height = s_PointShadowMapSize,
                .Attachments = { FramebufferTextureSpecification(shadowMap) },
                .DebugName = "Framebuffer Point Shadow Cubes",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);
            const nvrhi::TextureSubresourceSet pointShadowSubresources(0, 1, 0, MaxPointLights * 6);

            PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("shadowDepth"),
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Point Shadow Depth",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer(pointShadowSubresources)->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .Subresources = pointShadowSubresources,
                .ClearDepthOnLoad = true,
                .DepthClearValue = 1.0f,
            };

            m_PointShadowDepthPass = CreateRef<RenderPass>(renderPassSpec);

            // Point Shadow Depth Double Sided
            pipelineSpec.CullMode = nvrhi::RasterCullMode::None;
            m_PointShadowDepthDoubleSidedPipeline =
                CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer(pointShadowSubresources)->getFramebufferInfo());
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

        // Geometry
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::RGBA16_FLOAT, nvrhi::Format::RGBA16_FLOAT, nvrhi::Format::D32 },
                .DebugName = "Framebuffer Geometry",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            PipelineSpecification pipelineSpec{
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

            // Geometry Double Sided
            pipelineSpec.CullMode = nvrhi::RasterCullMode::None;
            m_GeometryDoubleSidedPipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo());
        }

        // Lighting
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA16_FLOAT },
                .DebugName = "Framebuffer Lighting",
            };

            const auto framebuffer = CreateRef<Framebuffer>(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("lighting"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Lighting",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            };

            m_LightingPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Skybox
        {
            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("skybox"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Skybox",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_LightingPass->GetFramebuffer()->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_LightingPass->GetFramebuffer(),
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
        m_ShadowDepthPass->SetInput(0, 2, m_LightsUB);
        m_ShadowDepthPass->SetInput(0, 1, m_DrawDataSB);
        m_ShadowDepthPass->SetInput(0, 2, m_MaterialDataSB);

        m_PointShadowDepthPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_PointShadowDepthPass->SetInput(0, 1, m_ShadowDepthUB);
        m_PointShadowDepthPass->SetInput(0, 2, m_LightsUB);
        m_PointShadowDepthPass->SetInput(0, 1, m_DrawDataSB);
        m_PointShadowDepthPass->SetInput(0, 2, m_MaterialDataSB);

        m_SsaoEvaluationPass->SetInput(
            0, 0,
            renderer->GetSampler(
                SamplerSpecification{
                    .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                }
            )
        );
        m_SsaoEvaluationPass->SetInput(0, 1, m_CameraUB);
        m_SsaoEvaluationPass->SetInput(0, 2, m_SsaoUB);
        m_SsaoBlurHorizontalPass->SetInput(
            0, 0,
            renderer->GetSampler(
                SamplerSpecification{
                    .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                }
            )
        );
        m_SsaoBlurHorizontalPass->SetInput(0, 1, m_CameraUB);

        m_GeometryPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_GeometryPass->SetInput(0, 1, m_DrawDataSB);
        m_GeometryPass->SetInput(0, 2, m_MaterialDataSB);
        m_GeometryPass->SetInput(0, 2, m_CameraUB);

        m_LightingPass->SetInput(
            0, 0,
            renderer->GetSampler(
                SamplerSpecification{ .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                      .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                      .AddressModeW = nvrhi::SamplerAddressMode::Clamp }
            )
        );
        m_LightingPass->SetInput(0, 1, m_ShadowDepthUB);
        m_LightingPass->SetInput(0, 2, m_CameraUB);
        m_LightingPass->SetInput(0, 3, m_LightsUB);
        m_LightingPass->SetInput(0, 4, m_EnvironmentUB);
        m_LightingPass->SetInput(0, 5, m_SsaoUB);

        m_SkyPass->SetInput(
            0, 0,
            renderer->GetSampler(
                SamplerSpecification{ .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                      .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                      .AddressModeW = nvrhi::SamplerAddressMode::Clamp }
            )
        );
        m_SkyPass->SetInput(0, 1, m_CameraUB);
        m_SkyPass->SetInput(0, 3, m_EnvironmentUB);

        m_WireframePass->SetInput(0, 0, m_WireframeInstanceSB);
        m_WireframePass->SetInput(0, 1, m_CameraUB);
        m_WireframePass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetDepthImage());

        m_TonemapPass->SetInput(
            0, 0,
            renderer->GetSampler(
                SamplerSpecification{
                    .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                }
            )
        );
    }

    auto SceneRenderer::RenderGui() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::RenderGui");

        const auto& app = Application::Get();
        if (!app.GetImGuiLayer())
            return;
        const auto& dm = DeviceManager::Get();
        const uint32_t frameIndex = dm->GetCurrentFrameIndex();
        EP_ASSERT(frameIndex < dm->GetMaxFramesInFlight());

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
            { .Pass = m_PointShadowDepthPass },
            Group(
                "SSAO",
                {
                    { .Pass = m_SsaoEvaluationPass },
                    { .Pass = m_SsaoBlurHorizontalPass },
                }
            ),
            { .Pass = m_GeometryPass },
            { .Pass = m_LightingPass },
            { .Pass = m_SkyPass },
            Group(
                "Bloom",
                {
                    { .Pass = m_BloomDownSamplePass },
                    { .Pass = m_BloomUpSamplePass },
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

        ImGui::SeparatorText("Camera");
        ImGui::Text("Position: x: %.2f, y: %.2f, z: %.2f", m_CameraData.Position.x, m_CameraData.Position.y, m_CameraData.Position.z);

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
        GatherWireframes();
        PrepareRenderData();

        Renderer::Submit(
            [this]()
            {
                m_RenderCommandBuffer->Begin();
            }
        );

        UploadRenderData();
        ShadowDepthPass();
        if (m_LightData.NumLights > 0)
            PointShadowDepthPass();
        GeometryPass();
        SsaoPass();
        LightingPass();
        SkyPass();
        BloomPass();
        TonemapPass();
        WireframePass();

        Renderer::Submit(
            [this]()
            {
                EP_GPU_COLLECT(m_RenderCommandBuffer);
                m_RenderCommandBuffer->End();
                m_RenderCommandBuffer->Submit();
            }
        );
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

    auto SceneRenderer::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, const float intensity, const float range)
        -> void
    {
        auto& light = m_PointLights.emplace_back();
        light.Position = glm::vec4(position, glm::max(range, 0.01f));
        light.Color = glm::vec4(color, intensity);
    }

    auto SceneRenderer::SubmitEnvironmentSettings(const EnvironmentSettings& environment) -> void
    {
        m_EnvironmentData.ZenithColor = glm::vec4(environment.ZenithColor, 1.0f);
        m_EnvironmentData.HorizonColor = glm::vec4(environment.HorizonColor, 1.0f);
        m_EnvironmentData.GroundColor = glm::vec4(environment.GroundColor, 1.0f);
        m_EnvironmentData.Params.x = environment.AmbientIntensity;
        m_EnvironmentData.Params.z = environment.Exposure;

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

        // Image is an asset; resolves through the registry like meshes do.
        const auto& image = Project::GetActive()->GetAssetManager()->GetOrLoadAsset<Image>(environment.SkyboxHandle);
        if (!image)
        {
            Log::Error("Failed to load skybox image for handle {}", static_cast<uint64_t>(environment.SkyboxHandle));
            return;
        }

        if (!image->IsLoaded.load(std::memory_order_acquire))
            return;

        EnsureIblResources();
        m_EnvironmentCube->Handle = environment.SkyboxHandle;

        BakeEnvironmentMap(image);

        const auto& renderer = DeviceManager::Get()->GetRenderer();

        m_EnvironmentData.Params.y = 1.0f;
        m_EnvironmentData.IBL0 = glm::uvec4(
            m_EnvironmentCube->GetBindlessIndex(), m_IrradianceCube->GetBindlessIndex(), m_PrefilterCube->GetBindlessIndex(),
            m_BrdfLut->GetBindlessIndex()
        );
        m_EnvironmentData.IBL1 = glm::uvec4(
            renderer
                ->GetSampler(
                    SamplerSpecification{ .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                          .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                          .AddressModeW = nvrhi::SamplerAddressMode::Clamp }
                )
                ->GetBindlessIndex(),
            0, 0, 0
        );
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

        m_SsaoEvaluationPass->Resize(m_Width, m_Height);
        m_SsaoBlurHorizontalPass->Resize(m_Width, m_Height);
        m_GeometryPass->Resize(m_Width, m_Height);
        m_LightingPass->Resize(m_Width, m_Height);
        m_SkyPass->Resize(m_Width, m_Height);
        m_TonemapPass->Resize(m_Width, m_Height);
        m_WireframePass->Resize(m_Width, m_Height);

        m_BloomPyramidFramebuffer->Resize(glm::max(1u, m_Width / 2u), glm::max(1u, m_Height / 2u));
        m_BloomMipLevels = glm::min(s_MaxBloomMipLevels, m_BloomPyramidFramebuffer->GetFinalImage()->GetMipLevels());
    }

    auto SceneRenderer::BeginSceneInternal() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginSceneInternal");

        Renderer::Submit(
            [this]()
            {
                std::memset(&m_ShadowDepthPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_PointShadowDepthPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_SsaoEvaluationPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_SsaoBlurHorizontalPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_GeometryPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_LightingPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_SkyPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_BloomDownSamplePass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_BloomUpSamplePass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_WireframePass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_TonemapPass->GetStatistics(), 0, sizeof(PassStatistics));
            }
        );

        m_DrawCommands.clear();
        m_PointLights.clear();
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
        constexpr auto directionalLightColor = glm::vec4(1.0f, 0.8f, 0.1f, 1.0f);
        constexpr auto directionalLightMarkerColor = glm::vec4(0.95f, 0.95f, 0.85f, 1.0f);
        constexpr auto meshWireframeColor = glm::vec4(0.45f, 0.63f, 0.95f, 1.0f);
        const auto& assetManager = project->GetAssetManager();

        if (m_LightData.HasDirectionalLight)
        {
            DrawCommand markerDraw{
                .Mesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Sphere)),
                .Color = directionalLightMarkerColor,
            };
            DrawCommand shaftDraw{
                .Mesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cylinder)),
                .Color = directionalLightColor,
            };
            DrawCommand headDraw{
                .Mesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cone)),
                .Color = directionalLightColor,
            };

            m_Scene->ForEachEntity(
                [&](const Entity entity) -> void
                {
                    if (!entity.HasComponent<DirectionalLightComponent>())
                        return;

                    const glm::mat4 worldTransform = m_Scene->GetWorldTransform(entity);
                    const glm::mat4 lightTransform =
                        glm::translate(glm::mat4(1.0f), glm::vec3(worldTransform[3])) * glm::mat4_cast(m_Scene->GetWorldRotation(entity));
                    markerDraw.Transforms.emplace_back(lightTransform * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f)));
                    shaftDraw.Transforms.emplace_back(
                        lightTransform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.6f, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(0.04f, 0.6f, 0.04f))
                    );
                    headDraw.Transforms.emplace_back(
                        lightTransform * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.4f, 0.0f)) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(0.16f, -0.2f, 0.16f))
                    );
                }
            );

            m_WireframeDrawCommands.emplace_back(std::move(markerDraw));
            m_WireframeDrawCommands.emplace_back(std::move(shaftDraw));
            m_WireframeDrawCommands.emplace_back(std::move(headDraw));
        }

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

    auto SceneRenderer::PrepareRenderData() -> void
    {
        EP_PROFILE_FN("SceneRenderer::PrepareRenderData");

        // Select closest point lights for rendering
        const uint32_t pointLightCount = static_cast<uint32_t>(m_PointLights.size());
        m_LightData.NumLights = glm::min(pointLightCount, MaxPointLights);

        if (pointLightCount > MaxPointLights)
        {
            std::map<float, PointLight> distances;

            for (size_t i = 0; i < m_PointLights.size(); i++)
            {
                const auto& light = m_PointLights.at(i);
                const float distance = glm::distance(glm::vec3(light.Position), glm::vec3(m_CameraData.Position));
                distances.insert({ distance, light });
            }

            auto lightIt = distances.cbegin();
            for (uint32_t i = 0; i < MaxPointLights; i++, lightIt++)
                m_LightData.Lights.at(i) = lightIt->second;
        }
        else
        {
            for (uint32_t i = 0; i < m_LightData.NumLights; i++)
                m_LightData.Lights.at(i) = m_PointLights.at(i);
        }

        // Shadow depth
        FillShadowData();

        // Ssao data
        m_SsaoData.Params = glm::vec4(m_SsaoSettings.Radius, m_SsaoSettings.Bias, m_SsaoSettings.Power, m_SsaoSettings.Intensity);
        m_SsaoData.InvSize = glm::vec4(1.0f / m_Width, 1.0f / m_Height, 0.0f, 0.0f);

        // Instance storage buffer
        m_InstanceTransforms.clear();
        for (auto& drawCmd : m_DrawCommands | std::views::values)
        {
            drawCmd.InstanceOffset = static_cast<uint32_t>(m_InstanceTransforms.size());
            m_InstanceTransforms.insert(m_InstanceTransforms.end(), drawCmd.Transforms.begin(), drawCmd.Transforms.end());
        }

        m_WireframeTransforms.clear();
        for (auto& draw : m_WireframeDrawCommands)
        {
            draw.InstanceOffset = static_cast<uint32_t>(m_WireframeTransforms.size());
            m_WireframeTransforms.insert(m_WireframeTransforms.end(), draw.Transforms.begin(), draw.Transforms.end());
        }

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
                            .DiffuseSamplerIndex = material->DiffuseSampler ? material->DiffuseSampler->GetBindlessIndex() : 0,
                            .NormalSamplerIndex = material->NormalSampler ? material->NormalSampler->GetBindlessIndex() : 0,
                            .RoughMetSamplerIndex = material->RoughMetSampler ? material->RoughMetSampler->GetBindlessIndex() : 0,
                            .AOSamplerIndex = material->AOSampler ? material->AOSampler->GetBindlessIndex() : 0,
                            .EmissiveSamplerIndex = material->EmissiveSampler ? material->EmissiveSampler->GetBindlessIndex() : 0,
                            .BaseColor = material->BaseColor,
                            .EmissiveFactor = material->EmissiveFactor,
                            .Metallic = material->Metallic,
                            .Roughness = material->Roughness,
                            .NormalScale = material->NormalScale,
                            .AlphaCutoff = material->AlphaCutoff,
                            .Flags = static_cast<uint32_t>(material->AlphaMode) | (material->DoubleSided ? 1u << 2u : 0u),
                        }
                    );
                }
            }
        }
    }

    auto SceneRenderer::UploadRenderData() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::UploadRenderData");

                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                // Uniform buffers
                const auto& shadowMap = m_ShadowDepthPass->GetFramebuffer()->GetDepthImage();
                m_ShadowDepthData.ShadowMapIndex = shadowMap->GetBindlessIndex(nvrhi::TextureSubresourceSet(0, 1, 0, s_ShadowCascadeCount));
                m_ShadowDepthData.ShadowSamplerIndex = DeviceManager::Get()
                                                           ->GetRenderer()
                                                           ->GetSampler(
                                                               SamplerSpecification{
                                                                   .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                                                   .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                                                   .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                                                                   .MinFilter = false,
                                                                   .MagFilter = false,
                                                                   .MipFilter = false,
                                                               }
                                                           )
                                                           ->GetBindlessIndex();
                const auto& pointShadowMap = m_PointShadowDepthPass->GetFramebuffer()->GetDepthImage();
                m_ShadowDepthData.PointShadowMapIndex =
                    pointShadowMap->GetBindlessIndex(nvrhi::TextureSubresourceSet(0, 1, 0, MaxPointLights * 6));
                m_ShadowDepthData.PointShadowSamplerIndex = m_ShadowDepthData.ShadowSamplerIndex;
                m_ShadowDepthData.PointShadowLightCount = m_LightData.NumLights;

                m_ShadowDepthUB->SetData(cmdList, &m_ShadowDepthData, sizeof(ShadowDepthData));
                m_SsaoUB->SetData(cmdList, &m_SsaoData, sizeof(SsaoData));
                m_CameraUB->SetData(cmdList, &m_CameraData, sizeof(CameraData));
                m_LightsUB->SetData(cmdList, &m_LightData, sizeof(LightData));
                m_EnvironmentUB->SetData(cmdList, &m_EnvironmentData, sizeof(EnvironmentData));

                // Storage buffers
                const uint64_t requiredSize = m_InstanceTransforms.size() * sizeof(glm::mat4);
                m_InstanceTransformsSB->SetData(cmdList, m_InstanceTransforms.data(), requiredSize);
                const uint64_t wireframeSize = m_WireframeTransforms.size() * sizeof(glm::mat4);
                m_WireframeInstanceSB->SetData(cmdList, m_WireframeTransforms.data(), wireframeSize);
                m_DrawDataSB->SetData(cmdList, m_DrawData.data(), m_DrawData.size() * sizeof(DrawData));
                m_MaterialDataSB->SetData(cmdList, m_MaterialData.data(), m_MaterialData.size() * sizeof(MaterialData));

                // Descriptors
                m_LightingPass->GetFramebuffer()->GetFinalImage()->RegisterBindlessIndex(nvrhi::TextureSubresourceSet(0, 1, 0, 1));

                const auto& bloomPyramid = m_BloomPyramidFramebuffer->GetFinalImage();
                for (uint32_t mip = 0; mip < m_BloomMipLevels; mip++)
                    bloomPyramid->RegisterBindlessIndex(nvrhi::TextureSubresourceSet(mip, 1, 0, 1));

                // Framebuffer attachments are recreated on resize.
                m_SsaoEvaluationPass->SetInput(0, 0, m_GeometryPass->GetFramebuffer()->GetDepthImage());
                m_SsaoEvaluationPass->SetInput(0, 1, m_GeometryPass->GetFramebuffer()->GetImage(1));
                m_SsaoBlurHorizontalPass->SetInput(0, 0, m_SsaoEvaluationPass->GetFramebuffer()->GetFinalImage());
                m_SsaoBlurHorizontalPass->SetInput(0, 1, m_GeometryPass->GetFramebuffer()->GetDepthImage());
                m_LightingPass->SetInput(0, 0, m_GeometryPass->GetFramebuffer()->GetImage(0));
                m_LightingPass->SetInput(0, 1, m_GeometryPass->GetFramebuffer()->GetImage(1));
                m_LightingPass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetImage(2));
                m_LightingPass->SetInput(0, 3, m_GeometryPass->GetFramebuffer()->GetDepthImage());
                m_LightingPass->SetInput(0, 4, m_SsaoBlurHorizontalPass->GetFramebuffer()->GetFinalImage());
                m_SkyPass->SetInput(0, 0, m_GeometryPass->GetFramebuffer()->GetDepthImage());
                m_TonemapPass->SetInput(0, 0, m_LightingPass->GetFramebuffer()->GetFinalImage());
                m_TonemapPass->SetInput(0, 1, m_BloomPyramidFramebuffer->GetFinalImage());
                m_WireframePass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetDepthImage());

                // Rebuild binding sets for any pass whose resource handles changed.
                m_ShadowDepthPass->Bake();
                m_PointShadowDepthPass->Bake();
                m_SsaoEvaluationPass->Bake();
                m_SsaoBlurHorizontalPass->Bake();
                m_GeometryPass->Bake();
                m_LightingPass->Bake();
                m_SkyPass->Bake();
                m_BloomDownSamplePass->Bake();
                m_BloomUpSamplePass->Bake();
                m_TonemapPass->Bake();
                m_WireframePass->Bake();
            }
        );
    }

    auto SceneRenderer::FillShadowData() -> void
    {
        EP_PROFILE_FN("SceneRenderer::FillShadowData")

        m_ShadowDepthData = {
            .DepthBiasTexels = s_ShadowBiasTexels,
            .NormalBiasTexels = s_ShadowNormalBiasTexels,
        };

        if (m_LightData.HasDirectionalLight && !m_DrawCommands.empty())
        {
            auto lightDir = glm::vec3(m_LightData.DirectionalLight.Direction);
            const float directionLengthSq = glm::dot(lightDir, lightDir);
            if (directionLengthSq > glm::epsilon<float>())
            {
                lightDir /= glm::sqrt(directionLengthSq);

                const float nearClip = glm::max(m_CameraData.NearClip, 0.001f);
                const float farClip = glm::max(m_CameraData.FarClip, nearClip);
                m_ShadowDepthData.ShadowDistance = farClip;
                if (farClip > nearClip)
                {

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
                                const glm::vec4 ndc(
                                    static_cast<float>(x) * 2.0f - 1.0f, static_cast<float>(y) * 2.0f - 1.0f, static_cast<float>(z), 1.0f
                                );
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
                        const float previousSplit = cascade == 0 ? nearClip : splits[cascade - 1];
                        m_ShadowDepthData.Cascades[cascade].WorldUnitsPerTexel = worldUnitsPerTexel;
                        m_ShadowDepthData.Cascades[cascade].TransitionStart = glm::mix(previousSplit, cascadeFar, 0.9f);
                        const glm::mat4 snapView = glm::lookAt(-lightDir, glm::vec3(0.0f), lightUp);
                        glm::vec4 snappedCenter = snapView * glm::vec4(center, 1.0f);
                        snappedCenter.x = glm::floor(snappedCenter.x / worldUnitsPerTexel) * worldUnitsPerTexel;
                        snappedCenter.y = glm::floor(snappedCenter.y / worldUnitsPerTexel) * worldUnitsPerTexel;
                        center = glm::vec3(glm::inverse(snapView) * snappedCenter);

                        float minimumCasterDistance = -radius;
                        for (const auto& drawCmd : m_DrawCommands | std::views::values)
                        {
                            if (!drawCmd.Mesh)
                                continue;

                            const AABB& bounds = drawCmd.Mesh->GetBounds();
                            if (!bounds.IsValid())
                                continue;

                            for (const auto& transform : drawCmd.Transforms)
                            {
                                glm::vec3 lightSpaceMin(std::numeric_limits<float>::max());
                                glm::vec3 lightSpaceMax(std::numeric_limits<float>::lowest());
                                float instanceMinimumDistance = std::numeric_limits<float>::max();

                                for (uint32_t cornerIndex = 0; cornerIndex < 8; cornerIndex++)
                                {
                                    const glm::vec3 localPosition = glm::vec3(
                                        (cornerIndex & 1u) != 0 ? bounds.Max.x : bounds.Min.x,
                                        (cornerIndex & 2u) != 0 ? bounds.Max.y : bounds.Min.y,
                                        (cornerIndex & 4u) != 0 ? bounds.Max.z : bounds.Min.z
                                    );

                                    const glm::vec3 worldPosition = glm::vec3(transform * glm::vec4(localPosition, 1.0f));
                                    const glm::vec3 relativePosition = worldPosition - center;
                                    const glm::vec3 lightSpacePosition = glm::vec3(snapView * glm::vec4(relativePosition, 0.0f));
                                    lightSpaceMin = glm::min(lightSpaceMin, lightSpacePosition);
                                    lightSpaceMax = glm::max(lightSpaceMax, lightSpacePosition);
                                    instanceMinimumDistance = glm::min(instanceMinimumDistance, glm::dot(relativePosition, lightDir));
                                    const bool overlapsCascade = lightSpaceMax.x >= -radius && lightSpaceMin.x <= radius &&
                                        lightSpaceMax.y >= -radius && lightSpaceMin.y <= radius;

                                    if (overlapsCascade)
                                        minimumCasterDistance = glm::min(minimumCasterDistance, instanceMinimumDistance);
                                }
                            }
                        }

                        const float depthPadding = glm::max(worldUnitsPerTexel * 4.0f, 0.1f);
                        const glm::vec3 lightPosition = center + lightDir * (minimumCasterDistance - depthPadding);
                        const glm::mat4 lightView = glm::lookAt(lightPosition, center, lightUp);
                        const float farPlane = radius - minimumCasterDistance + depthPadding * 2.0f;
                        const glm::mat4 lightProjection = glm::ortho(-radius, radius, -radius, radius, 0.0f, farPlane);
                        m_ShadowDepthData.Cascades[cascade].LightViewProjection = lightProjection * lightView;
                        cascadeNear = cascadeFar;
                    }
                }
            }
        }

        constexpr std::array directions = {
            glm::vec3(1.0f, 0.0f, 0.0f),  glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
        };
        constexpr std::array ups = {
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        };

        for (uint32_t i = 0; i < m_LightData.NumLights; i++)
        {
            const glm::vec3 position = glm::vec3(m_LightData.Lights[i].Position);
            const float range = m_LightData.Lights[i].Position.w;
            glm::mat4 proj = glm::perspective(glm::half_pi<float>(), 1.0f, s_PointShadowNearClip, range);
            proj[1][1] *= -1.0f;

            for (uint32_t face = 0; face < 6; face++)
            {
                const glm::mat4 view = glm::lookAt(position, position + directions.at(face), ups.at(face));
                m_ShadowDepthData.PointLightViewProjections[i * 6 + face] = proj * view;
            }
        }
    }

    auto SceneRenderer::ShadowDepthPass() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::ShadowDepthPass")
                EP_GPU_ZONE(m_RenderCommandBuffer, "ShadowDepthPass")

                struct PC
                {
                    uint32_t DrawIndex;
                    uint32_t ShadowType = 0;
                    uint32_t ProjectionCount = 0;
                } pushConstants{};

                auto& statistics = m_ShadowDepthPass->GetStatistics();
                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                m_RenderCommandBuffer->BeginTimerQuery(m_ShadowDepthPass->GetName());
                m_RenderCommandBuffer->BeginMarker(m_ShadowDepthPass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_ShadowDepthPass);

                auto& state = m_RenderCommandBuffer->GetGraphicsState();

                uint32_t drawIndex = 0;
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

                        for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                        {
                            pushConstants.DrawIndex = drawIndex++;

                            if (material->AlphaMode == MaterialAlphaMode::Blend)
                                continue;

                            state.pipeline = material->DoubleSided ? m_ShadowDepthDoubleSidedPipeline->GetPipeline()
                                                                   : m_ShadowDepthPass->GetPipeline()->GetPipeline();
                            m_RenderCommandBuffer->CommitGraphicsState();

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

                EP_ASSERT(drawIndex == m_DrawData.size());

                Renderer::EndRenderPass(m_RenderCommandBuffer);
                m_RenderCommandBuffer->EndMarker();
                m_RenderCommandBuffer->EndTimerQuery(m_ShadowDepthPass->GetName());
            }
        );
    }

    auto SceneRenderer::PointShadowDepthPass() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::PointShadowDepthPass");
                EP_GPU_ZONE(m_RenderCommandBuffer, "PointShadowDepthPass");

                struct PC
                {
                    uint32_t DrawIndex;
                    uint32_t ShadowType = 1;
                    uint32_t ProjectionCount;
                } pushConstants{};
                pushConstants.ProjectionCount = m_LightData.NumLights * 6;

                auto& statistics = m_PointShadowDepthPass->GetStatistics();
                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                m_RenderCommandBuffer->BeginTimerQuery(m_PointShadowDepthPass->GetName());
                m_RenderCommandBuffer->BeginMarker(m_PointShadowDepthPass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_PointShadowDepthPass);

                auto& state = m_RenderCommandBuffer->GetGraphicsState();

                uint32_t drawIndex = 0;
                for (const auto& drawCmd : m_DrawCommands | std::views::values)
                {
                    const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
                    if (instanceCount == 0)
                        continue;

                    // Each object instance is drawn once per cascade; the vertex shader derives the cascade
                    // and object index from SV_InstanceID and writes SV_RenderTargetArrayIndex.
                    const uint32_t layeredInstanceCount = instanceCount * pushConstants.ProjectionCount;

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

                        for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                        {
                            pushConstants.DrawIndex = drawIndex++;

                            if (material->AlphaMode == MaterialAlphaMode::Blend)
                                continue;

                            state.pipeline = material->DoubleSided ? m_PointShadowDepthDoubleSidedPipeline->GetPipeline()
                                                                   : m_PointShadowDepthPass->GetPipeline()->GetPipeline();
                            m_RenderCommandBuffer->CommitGraphicsState();

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

                EP_ASSERT(drawIndex == m_DrawData.size());

                Renderer::EndRenderPass(m_RenderCommandBuffer);
                m_RenderCommandBuffer->EndMarker();
                m_RenderCommandBuffer->EndTimerQuery(m_PointShadowDepthPass->GetName());
            }
        );
    }

    auto SceneRenderer::SsaoPass() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::SsaoPass")
                EP_GPU_ZONE(m_RenderCommandBuffer, "SsaoPass")

                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                // Explicit framebuffer transition
                cmdList->setTextureState(
                    m_GeometryPass->GetFramebuffer()->GetDepthImage()->GetTexture(), nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource
                );
                cmdList->setTextureState(
                    m_GeometryPass->GetFramebuffer()->GetImage(1)->GetTexture(), nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource
                );

                m_RenderCommandBuffer->BeginMarker("SSAO");

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

                m_RenderCommandBuffer->EndMarker();
            }
        );
    }

    auto SceneRenderer::GeometryPass() -> void
    {
        Renderer::Submit(
            [this]()
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

                        for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                        {
                            pushConstants.DrawIndex = drawIndex++;

                            // Blend primitives are rendered by transparency pass
                            if (material->AlphaMode == MaterialAlphaMode::Blend)
                                continue;

                            state.pipeline = material->DoubleSided ? m_GeometryDoubleSidedPipeline->GetPipeline()
                                                                   : m_GeometryPass->GetPipeline()->GetPipeline();
                            m_RenderCommandBuffer->CommitGraphicsState();

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
        );
    }

    auto SceneRenderer::LightingPass() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::LightingPass");
                EP_GPU_ZONE(m_RenderCommandBuffer, "LightingPass");

                auto& statistics = m_LightingPass->GetStatistics();
                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                cmdList->setTextureState(
                    m_ShadowDepthPass->GetFramebuffer()->GetFinalImage()->GetTexture(), nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource
                );
                cmdList->setTextureState(
                    m_PointShadowDepthPass->GetFramebuffer()->GetFinalImage()->GetTexture(), nvrhi::AllSubresources,
                    nvrhi::ResourceStates::ShaderResource
                );

                m_RenderCommandBuffer->BeginTimerQuery(m_LightingPass->GetName());
                m_RenderCommandBuffer->BeginMarker(m_LightingPass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_LightingPass);

                constexpr nvrhi::DrawArguments drawArgs{
                    .vertexCount = 3,
                    .instanceCount = 1,
                };
                cmdList->draw(drawArgs);

                statistics.DrawCalls++;
                statistics.Vertices += drawArgs.vertexCount;

                Renderer::EndRenderPass(m_RenderCommandBuffer);
                m_RenderCommandBuffer->EndMarker();
                m_RenderCommandBuffer->EndTimerQuery(m_LightingPass->GetName());
            }
        );
    }

    auto SceneRenderer::SkyPass() const -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::SkyPass");
                EP_GPU_ZONE(m_RenderCommandBuffer, "SkyPass");

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
        );
    }

    auto SceneRenderer::BloomPass() -> void
    {
        Renderer::Submit(
            [this]()
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
                const auto& sceneImage = m_LightingPass->GetFramebuffer()->GetFinalImage();
                const auto& pyramid = m_BloomPyramidFramebuffer->GetFinalImage();
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
                        pushConstants.Indices = glm::uvec4(
                            source->GetBindlessIndex(sourceSub),
                            DeviceManager::Get()
                                ->GetRenderer()
                                ->GetSampler(
                                    SamplerSpecification{
                                        .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                        .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                        .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                                    }
                                )
                                ->GetBindlessIndex(),
                            base ? 1u : 0u, 0u
                        );

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
                        pushConstants.Indices = glm::uvec4(
                            pyramid->GetBindlessIndex(sourceSub),
                            DeviceManager::Get()
                                ->GetRenderer()
                                ->GetSampler(
                                    SamplerSpecification{
                                        .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                        .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                        .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                                    }
                                )
                                ->GetBindlessIndex(),
                            0u, 0u
                        );

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

                m_RenderCommandBuffer->EndMarker();
            }
        );
    }

    auto SceneRenderer::TonemapPass() const -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::TonemapPass")
                EP_GPU_ZONE(m_RenderCommandBuffer, "TonemapPass")

                struct PC
                {
                    float Exposure = 1.0f;
                    float BloomIntensity = 0.0f;
                    glm::vec2 Padding{};
                } pushConstants{};
                pushConstants.Exposure = m_EnvironmentData.Params.z;
                pushConstants.BloomIntensity = m_BloomSettings.Intensity;

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
        );
    }

    auto SceneRenderer::WireframePass() const -> void
    {
        Renderer::Submit(
            [this]()
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
        );
    }

    auto SceneRenderer::EnsureIblResources() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EnsureIblResources")

        if (m_EnvironmentCube)
            return;

        const auto& renderer = DeviceManager::Get()->GetRenderer();

        m_EnvironmentCube = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
                .Width = s_IblEnvironmentSize,
                .Height = s_IblEnvironmentSize,
                .MipLevels = Image::CalculateMipLevels(s_IblEnvironmentSize, s_IblEnvironmentSize),
                .IsCubemap = true,
                .IsRenderTarget = true,
                .DebugName = "IBL Environment Cube",
            }
        );

        m_IrradianceCube = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
                .Width = s_IblIrradianceSize,
                .Height = s_IblIrradianceSize,
                .IsCubemap = true,
                .IsRenderTarget = true,
                .DebugName = "IBL Irradiance Cube",
            }
        );

        m_PrefilterCube = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
                .Width = s_IblPrefilterSize,
                .Height = s_IblPrefilterSize,
                .MipLevels = s_IblPrefilterMipLevels,
                .IsCubemap = true,
                .IsRenderTarget = true,
                .DebugName = "IBL Prefilter Cube",
            }
        );

        m_BrdfLut = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RG16_FLOAT,
                .Width = s_IblBrdfLutSize,
                .Height = s_IblBrdfLutSize,
                .IsRenderTarget = true,
                .DebugName = "IBL BRDF LUT",
            }
        );

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
        RecordIblPass(
            renderer->GetShader("iblEquirectToCube"), equirect,
            renderer->GetSampler(
                SamplerSpecification{
                    .AddressModeU = nvrhi::SamplerAddressMode::Wrap,
                    .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                }
            ),
            m_EnvironmentCube, facesUB, cmdBuffer, 0
        );

        for (uint32_t mip = 1; mip < m_EnvironmentCube->GetMipLevels(); mip++)
            RecordEnvironmentMipPass(m_EnvironmentCube, facesUB, cmdBuffer, mip);

        // Environment cube -> irradiance. Clamp for the cube convolution.
        RecordIblPass(
            renderer->GetShader("iblIrradiance"), m_EnvironmentCube,
            renderer->GetSampler(
                SamplerSpecification{
                    .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                    .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                }
            ),
            m_IrradianceCube, facesUB, cmdBuffer, 0, 0.0f, static_cast<float>(s_IblEnvironmentSize)
        );

        // Environment cube -> prefiltered specular, one mip per roughness. Clamp.
        for (uint32_t mip = 0; mip < s_IblPrefilterMipLevels; mip++)
        {
            const float roughness = static_cast<float>(mip) / static_cast<float>(s_IblPrefilterMipLevels - 1);
            RecordIblPass(
                renderer->GetShader("iblPrefilter"), m_EnvironmentCube,
                renderer->GetSampler(
                    SamplerSpecification{
                        .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                        .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                        .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                    }
                ),
                m_PrefilterCube, facesUB, cmdBuffer, mip, roughness, static_cast<float>(s_IblEnvironmentSize)
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

        const auto& renderer = DeviceManager::Get()->GetRenderer();
        const auto shader = renderer->GetShader("iblEnvironmentMip");

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
            .SamplerIndex = renderer
                                ->GetSampler(
                                    SamplerSpecification{
                                        .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                        .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                        .AddressModeW = nvrhi::SamplerAddressMode::Clamp,
                                    }
                                )
                                ->GetBindlessIndex(),
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
