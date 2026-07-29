#include "pch.h"
#include "Renderer/SceneRenderer.h"

#include "Core/Application.h"
#include "Project/Project.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Eppo
{
    namespace
    {
        constexpr uint32_t s_ShadowDepthMapSize = 2048;
        constexpr float s_ShadowBias = 0.0015f;
        constexpr float s_ShadowMinRadius = 1.0f;
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

        auto ExpandTransformedBounds(const AABB& source, AABB& destination, const glm::mat4& transform) -> void
        {
            if (!source.IsValid())
                return;

            const std::array corners{
                glm::vec3(source.Min.x, source.Min.y, source.Min.z), glm::vec3(source.Max.x, source.Min.y, source.Min.z),
                glm::vec3(source.Min.x, source.Max.y, source.Min.z), glm::vec3(source.Max.x, source.Max.y, source.Min.z),
                glm::vec3(source.Min.x, source.Min.y, source.Max.z), glm::vec3(source.Max.x, source.Min.y, source.Max.z),
                glm::vec3(source.Min.x, source.Max.y, source.Max.z), glm::vec3(source.Max.x, source.Max.y, source.Max.z),
            };

            for (const auto& corner : corners)
                destination.Expand(glm::vec3(transform * glm::vec4(corner, 1.0f)));
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
            const FramebufferSpecification framebufferSpec{
                .Width = s_ShadowDepthMapSize,
                .Height = s_ShadowDepthMapSize,
                .Attachments = { nvrhi::Format::D32 },
                .DebugName = "Framebuffer Shadow Depth",
            };

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("shadowDepth"),
                .Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
                .Width = s_ShadowDepthMapSize,
                .Height = s_ShadowDepthMapSize,
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Shadow Depth",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec),
                .ClearDepthOnLoad = true,
                .DepthClearValue = 1.0f,
            };

            m_ShadowDepthPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Geometry
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA16_FLOAT, nvrhi::Format::D32 },
                .DebugName = "Framebuffer Geometry",
            };

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("geometry"),
                .Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
                .Width = m_Width,
                .Height = m_Height,
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Geometry",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec),
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
                .Framebuffer = m_GeometryPass->GetPipeline()->GetSpecification().Framebuffer,
                .OwnsFramebuffer = false,
                .Width = m_Width,
                .Height = m_Height,
                .CullMode = nvrhi::RasterCullMode::None,
                .DepthTestEnable = true,
                .DepthWriteEnable = false,
                .DepthFunc = nvrhi::ComparisonFunc::LessOrEqual,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Skybox",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec),
            };

            m_SkyPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Tonemap
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM },
                .DebugName = "Framebuffer Tonemap",
            };

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("tonemap"),
                .Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
                .Width = m_Width,
                .Height = m_Height,
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Tonemap",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec),
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            };

            m_TonemapPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Wireframe
        {
            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("wireframe"),
                .Framebuffer = m_TonemapPass->GetFramebuffer(),
                .OwnsFramebuffer = false,
                .Width = m_Width,
                .Height = m_Height,
                .CullMode = nvrhi::RasterCullMode::None,
                .FillMode = nvrhi::RasterFillMode::Wireframe,
                .DepthTestEnable = false,
                .DepthWriteEnable = false,
                .DepthBias = -1,
                .SlopeScaledDepthBias = -1.0f,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Wireframe",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec),
            };

            m_WireframePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Uniform buffers
        m_ShadowDepthUB = CreateRef<UniformBuffer>(sizeof(ShadowDepthData), "UniformBuffer Shadow Depth");
        m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Camera");
        m_LightsUB = CreateRef<UniformBuffer>(sizeof(LightData), "UniformBuffer Lights");
        m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), "UniformBuffer Environment");

        m_InstanceTransformsSB = CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Instance Transforms");
        m_WireframeInstanceSB =
            CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Wireframe Instance Transforms");
        m_DrawDataSB = CreateRef<StorageBuffer>(sizeof(DrawData), sizeof(DrawData), "StorageBuffer Draw Data");
        m_MaterialDataSB = CreateRef<StorageBuffer>(sizeof(MaterialData), sizeof(MaterialData), "StorageBuffer Material Data");

        // Inputs retain their resources and resolve current GPU handles whenever a pass bakes.
        m_ShadowDepthPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_ShadowDepthPass->SetInput(0, 1, m_ShadowDepthUB);

        m_GeometryPass->SetInput(0, 0, m_WrapAllFiltersTrueSampler);
        m_GeometryPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_GeometryPass->SetInput(0, 1, m_DrawDataSB);
        m_GeometryPass->SetInput(0, 2, m_MaterialDataSB);
        m_GeometryPass->SetInput(0, 1, m_ShadowDepthUB);
        m_GeometryPass->SetInput(0, 2, m_CameraUB);
        m_GeometryPass->SetInput(0, 3, m_LightsUB);
        m_GeometryPass->SetInput(0, 4, m_EnvironmentUB);

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

        // One collapsible row per scene pass: its GPU time plus draw-call breakdown.
        const auto renderPass = [](const char* name, const PassStatistics& stats, float timeMs) -> void
        {
            if (!ImGui::TreeNodeEx(name, 0, "%s: %.2fms", name, timeMs))
                return;

            ImGui::Text("Draw calls: %u", stats.DrawCalls);
            ImGui::Text("Meshes: %u", stats.Meshes);
            ImGui::Text("Submeshes: %u", stats.Submeshes);
            ImGui::Text("Instances: %u", stats.Instances);
            ImGui::Text("Vertices: %u", stats.Vertices);
            ImGui::Text("Indices: %u", stats.Indices);
            ImGui::TreePop();
        };

        ImGui::Begin("Scene Renderer");

        // Scene passes and their subtotal.
        ImGui::SeparatorText("Scene");
        const auto& shadowDepthStats = m_ShadowDepthPass->GetStatistics();
        const auto& geometryStats = m_GeometryPass->GetStatistics();
        const auto& skyStats = m_SkyPass->GetStatistics();
        const auto& wireframeStats = m_WireframePass->GetStatistics();
        const auto& tonemapStats = m_TonemapPass->GetStatistics();

        renderPass(
            m_ShadowDepthPass->GetName().c_str(), shadowDepthStats,
            m_RenderCommandBuffer->GetTimeMs(m_ShadowDepthPass->GetName(), frameIndex)
        );
        renderPass(
            m_GeometryPass->GetName().c_str(), geometryStats, m_RenderCommandBuffer->GetTimeMs(m_GeometryPass->GetName(), frameIndex)
        );
        renderPass(m_SkyPass->GetName().c_str(), skyStats, m_RenderCommandBuffer->GetTimeMs(m_SkyPass->GetName(), frameIndex));
        renderPass(m_TonemapPass->GetName().c_str(), tonemapStats, m_RenderCommandBuffer->GetTimeMs(m_TonemapPass->GetName(), frameIndex));
        renderPass(
            m_WireframePass->GetName().c_str(), wireframeStats, m_RenderCommandBuffer->GetTimeMs(m_WireframePass->GetName(), frameIndex)
        );

        PassStatistics sceneStats;
        sceneStats += shadowDepthStats;
        sceneStats += geometryStats;
        sceneStats += skyStats;
        sceneStats += wireframeStats;
        sceneStats += tonemapStats;
        const float sceneTime = m_RenderCommandBuffer->GetTimeMs(m_GeometryPass->GetName(), frameIndex) +
            m_RenderCommandBuffer->GetTimeMs(m_SkyPass->GetName(), frameIndex) +
            m_RenderCommandBuffer->GetTimeMs(m_WireframePass->GetName(), frameIndex) +
            m_RenderCommandBuffer->GetTimeMs(m_TonemapPass->GetName(), frameIndex);
        ImGui::Text("Scene total: %u draw calls, %.2fms", sceneStats.DrawCalls, sceneTime);

        // UI is tracked and reported separately from the scene.
        ImGui::SeparatorText("UI");
        const PassStatistics uiStats = imguiRenderer->GetStats();
        ImGui::Text("UI: %.2fms", imguiRenderer->GetGPUTime(frameIndex));
        ImGui::Text("Draw calls: %u", uiStats.DrawCalls);
        ImGui::Text("Vertices: %u", uiStats.Vertices);
        ImGui::Text("Indices: %u", uiStats.Indices);

        // Everything on screen: scene passes plus UI.
        const std::string totalLabel = std::format("Total: {:.2f}ms", sceneTime + imguiRenderer->GetGPUTime(frameIndex));
        ImGui::SeparatorText(totalLabel.c_str());
        ImGui::Text("Draw calls: %u", sceneStats.DrawCalls + uiStats.DrawCalls);
        ImGui::Text("Vertices: %u", sceneStats.Vertices + uiStats.Vertices);
        ImGui::Text("Indices: %u", sceneStats.Indices + uiStats.Indices);

        ImGui::End();
    }

    auto SceneRenderer::BeginScene(const EditorCamera& camera) -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginScene")

        m_CameraData.View = camera.GetViewMatrix();
        m_CameraData.Projection = camera.GetProjectionMatrix();
        m_CameraData.ViewProjection = camera.GetViewProjection();
        m_CameraData.Position = glm::vec4(camera.GetPosition(), 0.0f);

        BeginSceneInternal();
    }

    auto SceneRenderer::BeginScene(const SceneCamera& camera, const glm::mat4& transform) -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginScene")

        m_CameraData.View = glm::inverse(transform);
        m_CameraData.Projection = camera.GetProjectionMatrix();
        m_CameraData.ViewProjection = m_CameraData.Projection * m_CameraData.View;
        m_CameraData.Position = glm::vec4(glm::vec3(transform[3]), 0.0f);

        BeginSceneInternal();
    }

    auto SceneRenderer::EndScene() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EndScene")

        EnsureColliderMeshes();

        m_RenderCommandBuffer->Begin();
        PrepareRender();

        ShadowDepthPass();
        GeometryPass();
        SkyPass();
        TonemapPass();
        WireframePass();

        m_RenderCommandBuffer->End();
        m_RenderCommandBuffer->Submit();
    }

    auto SceneRenderer::GetFinalImage() const -> const Ref<Image>&
    {
        return m_TonemapPass->GetPipeline()->GetSpecification().Framebuffer->GetFinalImage();
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

    auto SceneRenderer::SubmitEnvironment(const EnvironmentSettings& environment) -> void
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

    auto SceneRenderer::Resize(const uint32_t width, const uint32_t height) -> void
    {
        EP_PROFILE_FN("SceneRenderer::Resize")

        if (m_Width == width && m_Height == height)
            return;

        m_Width = width;
        m_Height = height;

        m_GeometryPass->Resize(m_Width, m_Height);
        m_SkyPass->Resize(m_Width, m_Height);
        m_TonemapPass->Resize(m_Width, m_Height);
        m_WireframePass->Resize(m_Width, m_Height);
    }

    auto SceneRenderer::BeginSceneInternal() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginSceneInternal")

        std::memset(&m_ShadowDepthPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_GeometryPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SkyPass->GetStatistics(), 0, sizeof(PassStatistics));
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
        m_TonemapPass->SetInput(0, 0, m_GeometryPass->GetFramebuffer()->GetFinalImage());
        m_WireframePass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetDepthImage());

        // Rebuild binding sets for any pass whose resource handles changed.
        m_ShadowDepthPass->Bake();
        m_GeometryPass->Bake();
        m_SkyPass->Bake();
        m_TonemapPass->Bake();
        m_WireframePass->Bake();
    }

    auto SceneRenderer::FillShadowData() -> void
    {
        EP_PROFILE_FN("SceneRenderer::FillShadowData")

        m_ShadowDepthData = {
            .LightViewProjection = glm::mat4(1.0f),
            .Params = glm::vec4(s_ShadowBias, 1.0f / static_cast<float>(s_ShadowDepthMapSize), 0.0f, 0.0f),
        };

        if (m_LightData.HasDirectionalLight == 0 || m_DrawCommands.empty())
            return;

        AABB sceneBounds;
        for (const auto& drawCmd : m_DrawCommands | std::views::values)
        {
            if (!drawCmd.Mesh)
                continue;

            for (const auto& transform : drawCmd.Transforms)
                ExpandTransformedBounds(drawCmd.Mesh->GetBounds(), sceneBounds, transform);
        }

        if (!sceneBounds.IsValid())
        {
            Log::Warn("Scene bounds were invalid while trying to fill shadow data!");
            return;
        }

        auto lightDirection = glm::vec3(m_LightData.DirectionalLight.Direction);
        const float directionLengthSq = glm::dot(lightDirection, lightDirection);
        if (directionLengthSq <= glm::epsilon<float>())
            return;
        lightDirection /= glm::sqrt(directionLengthSq);

        const glm::vec3 center = sceneBounds.GetCenter();
        const float radius = glm::max(glm::length(sceneBounds.GetHalfExtent()) * 1.05f, s_ShadowMinRadius);
        const float depthPadding = glm::max(radius * 0.1f, 0.1f);

        constexpr glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 lightUp = glm::abs(glm::dot(lightDirection, worldUp)) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : worldUp;
        const glm::vec3 lightPosition = center - lightDirection * (radius + depthPadding);
        const glm::mat4 lightView = glm::lookAt(lightPosition, center, lightUp);
        const glm::mat4 lightProjection = glm::ortho(-radius, radius, -radius, radius, depthPadding, radius * 2.0f + depthPadding);

        const auto& shadowMap = m_ShadowDepthPass->GetFramebuffer()->GetDepthImage();
        m_ShadowDepthData.LightViewProjection = lightProjection * lightView;
        m_ShadowDepthData.Indices = glm::uvec4(shadowMap->GetBindlessIndex(), m_ClampAllFiltersFalseSampler->GetBindlessIndex(), 1u, 0u);
    }

    auto SceneRenderer::ShadowDepthPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::ShadowDepthPass")

        if (m_ShadowDepthData.Indices.z == 0)
        {
            m_RenderCommandBuffer->BeginTimerQuery(m_ShadowDepthPass->GetName());
            m_RenderCommandBuffer->EndTimerQuery(m_ShadowDepthPass->GetName());
            return;
        }

        struct PC
        {
            glm::mat4 Transform;
            uint32_t InstanceOffset;
        } pushConstants{};

        auto& statistics = m_ShadowDepthPass->GetStatistics();

        m_RenderCommandBuffer->BeginTimerQuery(m_ShadowDepthPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_ShadowDepthPass);

        auto& state = m_RenderCommandBuffer->GetGraphicsState();

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

                pushConstants.Transform = submesh.LocalTransform;
                pushConstants.InstanceOffset = drawCmd.InstanceOffset;

                for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                {
                    m_RenderCommandBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(PC));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = instanceCount,
                        .startIndexLocation = firstIndex,
                        .startVertexLocation = firstVertex,
                    };

                    m_RenderCommandBuffer->GetCommandList()->drawIndexed(drawArgs);

                    statistics.DrawCalls++;
                    statistics.Vertices += static_cast<uint32_t>(vertexCount) * instanceCount;
                    statistics.Indices += static_cast<uint32_t>(indexCount) * instanceCount;
                }
                statistics.Submeshes++;
            }
            statistics.Instances += instanceCount;
            statistics.Meshes++;
        }

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndTimerQuery(m_ShadowDepthPass->GetName());
    }

    auto SceneRenderer::GeometryPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::GeometryPass")

        struct PC
        {
            uint32_t DrawIndex;
        } pushConstants{};

        auto& statistics = m_GeometryPass->GetStatistics();

        m_RenderCommandBuffer->BeginTimerQuery(m_GeometryPass->GetName());
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
                    m_RenderCommandBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(PC));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = instanceCount,
                        .startIndexLocation = firstIndex,
                        .startVertexLocation = firstVertex,
                    };

                    m_RenderCommandBuffer->GetCommandList()->drawIndexed(drawArgs);

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
        m_RenderCommandBuffer->EndTimerQuery(m_GeometryPass->GetName());
    }

    auto SceneRenderer::SkyPass() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::SkyPass")

        auto& statistics = m_SkyPass->GetStatistics();

        m_RenderCommandBuffer->BeginTimerQuery(m_SkyPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_SkyPass);

        constexpr nvrhi::DrawArguments drawArgs{
            .vertexCount = 3,
            .instanceCount = 1,
        };
        m_RenderCommandBuffer->GetCommandList()->draw(drawArgs);

        statistics.DrawCalls++;
        statistics.Vertices += drawArgs.vertexCount;

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndTimerQuery(m_SkyPass->GetName());
    }

    auto SceneRenderer::WireframePass() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::WireframePass")

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

        m_RenderCommandBuffer->BeginTimerQuery(m_WireframePass->GetName());
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
                    m_RenderCommandBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(PC));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = instanceCount,
                        .startIndexLocation = firstIndex,
                        .startVertexLocation = firstVertex,
                    };

                    m_RenderCommandBuffer->GetCommandList()->drawIndexed(drawArgs);

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
        m_RenderCommandBuffer->EndTimerQuery(m_WireframePass->GetName());
    }

    auto SceneRenderer::TonemapPass() const -> void
    {
        EP_PROFILE_FN("SceneRenderer::TonemapPass")

        constexpr struct PC
        {
            float Exposure = 1.0f;
        } pushConstants{};

        auto& statistics = m_TonemapPass->GetStatistics();

        m_RenderCommandBuffer->BeginTimerQuery(m_TonemapPass->GetName());
        Renderer::BeginRenderPass(m_RenderCommandBuffer, m_TonemapPass);

        m_RenderCommandBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(PC));

        constexpr nvrhi::DrawArguments drawArgs{
            .vertexCount = 3,
            .instanceCount = 1,
        };
        m_RenderCommandBuffer->GetCommandList()->draw(drawArgs);

        statistics.DrawCalls++;
        statistics.Vertices += drawArgs.vertexCount;

        Renderer::EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndTimerQuery(m_TonemapPass->GetName());
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
            .ExistingImage = { .Image = m_BrdfLut },
            .DebugName = "IBL BRDF LUT Framebuffer",
        });

        const auto pipeline = CreateRef<Pipeline>(PipelineSpecification{
            .Shader = renderer->GetShader("iblBrdfLut"),
            .Framebuffer = framebuffer,
            .Width = s_IblBrdfLutSize,
            .Height = s_IblBrdfLutSize,
            .CullMode = nvrhi::RasterCullMode::None,
        });

        const auto pass = CreateRef<RenderPass>(RenderPassSpecification{ .Name = "IBL BRDF LUT", .Pipeline = pipeline });
        pass->Bake();

        const auto cmdBuffer = CreateRef<RenderCommandBuffer>();
        cmdBuffer->Begin();
        Renderer::BeginRenderPass(cmdBuffer, pass);

        constexpr nvrhi::DrawArguments drawArgs{ .vertexCount = 3, .instanceCount = 1 };
        cmdBuffer->GetCommandList()->draw(drawArgs);

        Renderer::EndRenderPass(cmdBuffer);
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

        const auto cmdBuffer = CreateRef<RenderCommandBuffer>();
        cmdBuffer->Begin();

        facesUB->SetData(cmdBuffer->GetCommandList(), &inverseViewProjection, sizeof(glm::mat4) * 6);

        // Equirect -> environment cube. Wrap sampler so the atan2 longitude seam wraps cleanly.
        RecordIblPass(
            renderer->GetShader("iblEquirectToCube"), equirect, m_EquirectSampler, m_EnvironmentCube, facesUB, cmdBuffer, 0,
            s_IblEnvironmentSize
        );

        // Environment cube -> irradiance. Clamp for the cube convolution.
        RecordIblPass(
            renderer->GetShader("iblIrradiance"), m_EnvironmentCube, m_ClampAllFiltersTrueSampler, m_IrradianceCube, facesUB, cmdBuffer, 0,
            s_IblIrradianceSize
        );

        // Environment cube -> prefiltered specular, one mip per roughness. Clamp.
        for (uint32_t mip = 0; mip < s_IblPrefilterMipLevels; mip++)
        {
            const uint32_t mipSize = s_IblPrefilterSize >> mip;
            const float roughness = static_cast<float>(mip) / static_cast<float>(s_IblPrefilterMipLevels - 1);
            RecordIblPass(
                renderer->GetShader("iblPrefilter"), m_EnvironmentCube, m_ClampAllFiltersTrueSampler, m_PrefilterCube, facesUB, cmdBuffer,
                mip, mipSize, roughness, static_cast<float>(s_IblEnvironmentSize)
            );
        }

        cmdBuffer->End();
        cmdBuffer->Submit();
        DeviceManager::Get()->GetDevice()->waitForIdle();
    }

    auto SceneRenderer::RecordIblPass(
        const Ref<Shader>& shader, const Ref<Image>& source, const Ref<Sampler>& sampler, const Ref<Image>& target,
        const Ref<UniformBuffer>& facesUB, const Ref<RenderCommandBuffer>& cmdBuffer, const uint32_t mipLevel, const uint32_t size,
        const float roughness, const float envMapSize
    ) -> void
    {
        const auto framebuffer = CreateRef<Framebuffer>(FramebufferSpecification{
            .Width = size,
            .Height = size,
            .ExistingImage = { .Image = target, .MipLevel = mipLevel },
            .DebugName = "IBL Bake Framebuffer",
        });

        const auto pipeline = CreateRef<Pipeline>(PipelineSpecification{
            .Shader = shader,
            .Framebuffer = framebuffer,
            .Width = size,
            .Height = size,
            .CullMode = nvrhi::RasterCullMode::None,
        });

        const auto pass = CreateRef<RenderPass>(RenderPassSpecification{ .Name = "IBL Bake", .Pipeline = pipeline });

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

        Renderer::BeginRenderPass(cmdBuffer, pass);

        // Only the prefilter shader uses the push block; dxc strips it from the others, leaving no range.
        if (shader->GetPushConstants().Size > 0)
            cmdBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(PC));

        constexpr nvrhi::DrawArguments drawArgs{ .vertexCount = 3, .instanceCount = 6 };
        cmdBuffer->GetCommandList()->draw(drawArgs);

        Renderer::EndRenderPass(cmdBuffer);
    }
}
