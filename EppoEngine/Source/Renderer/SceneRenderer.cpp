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
        constexpr uint32_t s_IblEnvironmentSize = 512;

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

        m_RenderCommandBuffer = Ref<RenderCommandBuffer>::Create();

        // Create render passes
        // Opaque Forward
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA16_FLOAT, nvrhi::Format::D32 },
                .DebugName = "Framebuffer Opaque Forward",
            };

            const auto framebuffer = Ref<Framebuffer>::Create(framebufferSpec);

            PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("opaqueForward"),
                .CullMode = nvrhi::RasterCullMode::Front,
                .DepthTestEnable = true,
                .DepthWriteEnable = true,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Opaque Forward",
                .Pipeline = Ref<Pipeline>::Create(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                .ClearDepthOnLoad = true,
            };

            m_OpaqueForwardPass = Ref<RenderPass>::Create(renderPassSpec);

            // Opaque Forward Double Sided
            pipelineSpec.CullMode = nvrhi::RasterCullMode::None;
            m_OpaqueForwardDoubleSidedPipeline = Ref<Pipeline>::Create(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo());
        }

        // Skybox
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { FramebufferTextureSpecification(m_OpaqueForwardPass->GetFramebuffer()->GetFinalImage()) },
                .DebugName = "Framebuffer Skybox",
            };

            const auto framebuffer = Ref<Framebuffer>::Create(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("skybox"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Skybox",
                .Pipeline = Ref<Pipeline>::Create(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
            };

            m_SkyPass = Ref<RenderPass>::Create(renderPassSpec);
        }

        // Display Conversion
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM },
                .DebugName = "Framebuffer Display Conversion",
            };

            const auto framebuffer = Ref<Framebuffer>::Create(framebufferSpec);

            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("displayConversion"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Display Conversion",
                .Pipeline = Ref<Pipeline>::Create(pipelineSpec, framebuffer->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = framebuffer,
                .ClearColorOnLoad = true,
                .ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            };

            m_DisplayConversionPass = Ref<RenderPass>::Create(renderPassSpec);
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
                .Pipeline =
                    Ref<Pipeline>::Create(pipelineSpec, m_DisplayConversionPass->GetFramebuffer()->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_DisplayConversionPass->GetFramebuffer(),
                .OwnsFramebuffer = false,
            };

            m_WireframePass = Ref<RenderPass>::Create(renderPassSpec);
        }

        // Uniform buffers
        m_CameraUB = Ref<UniformBuffer>::Create(sizeof(CameraData), "UniformBuffer Camera");
        m_EnvironmentUB = Ref<UniformBuffer>::Create(sizeof(EnvironmentData), "UniformBuffer Environment");

        m_InstanceTransformsSB = Ref<StorageBuffer>::Create(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Instance Transforms");
        m_WireframeInstanceSB =
            Ref<StorageBuffer>::Create(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Wireframe Instance Transforms");
        m_DrawDataSB = Ref<StorageBuffer>::Create(sizeof(DrawData), sizeof(DrawData), "StorageBuffer Draw Data");
        m_MaterialDataSB = Ref<StorageBuffer>::Create(sizeof(MaterialData), sizeof(MaterialData), "StorageBuffer Material Data");

        m_OpaqueForwardPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_OpaqueForwardPass->SetInput(0, 1, m_DrawDataSB);
        m_OpaqueForwardPass->SetInput(0, 2, m_MaterialDataSB);
        m_OpaqueForwardPass->SetInput(0, 1, m_CameraUB);
        m_OpaqueForwardPass->SetInput(0, 2, m_EnvironmentUB);

        m_SkyPass->SetInput(
            0, 0,
            renderer->GetSampler(
                SamplerSpecification{ .AddressModeU = nvrhi::SamplerAddressMode::Clamp,
                                      .AddressModeV = nvrhi::SamplerAddressMode::Clamp,
                                      .AddressModeW = nvrhi::SamplerAddressMode::Clamp }
            )
        );
        m_SkyPass->SetInput(0, 0, m_CameraUB);
        m_SkyPass->SetInput(0, 1, m_EnvironmentUB);

        m_WireframePass->SetInput(0, 0, m_WireframeInstanceSB);
        m_WireframePass->SetInput(0, 1, m_CameraUB);
        m_WireframePass->SetInput(0, 1, m_OpaqueForwardPass->GetFramebuffer()->GetDepthImage());

        m_DisplayConversionPass->SetInput(
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
            { .Pass = m_OpaqueForwardPass },
            { .Pass = m_SkyPass },
            { .Pass = m_DisplayConversionPass },
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
        OpaqueForwardPass();
        SkyPass();
        DisplayConversionPass();
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
        return m_DisplayConversionPass->GetFramebuffer()->GetFinalImage();
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
            Ref<AssetManager> assetManager = Project::GetActive()->GetAssetManager();
            const Ref<Mesh> mesh = assetManager->GetOrLoadAsset(meshHandle).As<Mesh>();

            const DrawCommand cmd{
                .Mesh = mesh,
                .Transforms = { transform },
            };

            m_DrawCommands[key] = cmd;
        }
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
        Ref<AssetManager> assetManager = Project::GetActive()->GetAssetManager();
        const Ref<Image> image = assetManager->GetOrLoadAsset(environment.SkyboxHandle).As<Image>();
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
        m_EnvironmentData.IBL0 = glm::uvec4(m_EnvironmentCube->GetBindlessIndex(), 0, 0, 0);
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

    auto SceneRenderer::Resize(const uint32_t width, const uint32_t height) -> void
    {
        EP_PROFILE_FN("SceneRenderer::Resize")

        if (width == 0 || height || 0 || (m_Width == width && m_Height == height))
            return;

        m_Width = width;
        m_Height = height;

        m_OpaqueForwardPass->Resize(m_Width, m_Height);
        m_SkyPass->Resize(m_Width, m_Height);
        m_DisplayConversionPass->Resize(m_Width, m_Height);
        m_WireframePass->Resize(m_Width, m_Height);
    }

    auto SceneRenderer::SetScene(const Ref<Scene>& scene) -> void
    {
        if (scene == m_Scene)
            return;

        m_Scene = scene;
        InvalidateSceneState();
    }

    auto SceneRenderer::BeginSceneInternal() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginSceneInternal");

        Renderer::Submit(
            [this]()
            {
                std::memset(&m_OpaqueForwardPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_SkyPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_DisplayConversionPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_WireframePass->GetStatistics(), 0, sizeof(PassStatistics));
            }
        );

        m_DrawCommands.clear();

        m_CameraData.InverseViewProjection = glm::inverse(m_CameraData.ViewProjection);
    }

    auto SceneRenderer::InvalidateSceneState() -> void
    {
        EP_PROFILE_FN("SceneRenderer::InvalidateSceneState");

        m_DrawCommands.clear();
        m_DrawData.clear();
        m_MaterialData.clear();
        m_InstanceTransforms.clear();
        m_WireframeTransforms.clear();
        m_WireframeDrawCommands.clear();
        m_HighlightedEntity = {};
        m_EnvironmentData = {};

        if (m_EnvironmentCube)
            m_EnvironmentCube->Handle = 0;
    }

    auto SceneRenderer::EnsureColliderMeshes() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EnsureColliderMeshes")

        if (!m_DebugRenderingEnabled || (!m_ShowColliders && !m_HighlightedEntity))
            return;

        const auto& project = Project::GetActive();
        if (!project)
            return;

        Ref<AssetManager> assetManager = project->GetAssetManager();
        if (!m_BoxColliderMesh)
            m_BoxColliderMesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cube)).As<Mesh>();

        if (!m_SphereColliderMesh)
            m_SphereColliderMesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Sphere)).As<Mesh>();
        if (!m_CapsuleColliderMesh)
            m_CapsuleColliderMesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Capsule)).As<Mesh>();
        if (!m_CylinderColliderMesh)
            m_CylinderColliderMesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cylinder)).As<Mesh>();
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
        Ref<AssetManager> assetManager = project->GetAssetManager();

        if (m_Scene)
        {
            DrawCommand markerDraw{
                .Mesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Sphere)).As<Mesh>(),
                .Color = directionalLightMarkerColor,
            };
            DrawCommand shaftDraw{
                .Mesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cylinder)).As<Mesh>(),
                .Color = directionalLightColor,
            };
            DrawCommand headDraw{
                .Mesh = assetManager->GetOrLoadAsset(static_cast<uint64_t>(MeshPrimitiveType::Cone)).As<Mesh>(),
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
                const Ref<Mesh> mesh = assetManager->GetOrLoadAsset(mc.MeshHandle).As<Mesh>();
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
                m_CameraUB->SetData(cmdList, &m_CameraData, sizeof(CameraData));
                m_EnvironmentUB->SetData(cmdList, &m_EnvironmentData, sizeof(EnvironmentData));

                // Storage buffers
                const uint64_t requiredSize = m_InstanceTransforms.size() * sizeof(glm::mat4);
                m_InstanceTransformsSB->SetData(cmdList, m_InstanceTransforms.data(), requiredSize);
                const uint64_t wireframeSize = m_WireframeTransforms.size() * sizeof(glm::mat4);
                m_WireframeInstanceSB->SetData(cmdList, m_WireframeTransforms.data(), wireframeSize);
                m_DrawDataSB->SetData(cmdList, m_DrawData.data(), m_DrawData.size() * sizeof(DrawData));
                m_MaterialDataSB->SetData(cmdList, m_MaterialData.data(), m_MaterialData.size() * sizeof(MaterialData));

                // Descriptors
                const auto& framebuffer = m_OpaqueForwardPass->GetFramebuffer();
                m_SkyPass->SetInput(0, 0, framebuffer->GetDepthImage());
                m_DisplayConversionPass->SetInput(0, 0, framebuffer->GetFinalImage());
                m_WireframePass->SetInput(0, 1, framebuffer->GetDepthImage());

                // Rebuild binding sets for any pass whose resource handles changed.
                m_OpaqueForwardPass->Bake();
                m_SkyPass->Bake();
                m_DisplayConversionPass->Bake();
                m_WireframePass->Bake();
            }
        );
    }

    auto SceneRenderer::OpaqueForwardPass() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::OpaqueForwardPass")
                EP_GPU_ZONE(m_RenderCommandBuffer, "OpaqueForwardPass")

                struct PC
                {
                    uint32_t DrawIndex;
                } pushConstants{};

                auto& statistics = m_OpaqueForwardPass->GetStatistics();
                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                m_RenderCommandBuffer->BeginTimerQuery(m_OpaqueForwardPass->GetName());
                m_RenderCommandBuffer->BeginMarker(m_OpaqueForwardPass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_OpaqueForwardPass);

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

                            state.pipeline = material->DoubleSided ? m_OpaqueForwardDoubleSidedPipeline->GetPipeline()
                                                                   : m_OpaqueForwardPass->GetPipeline()->GetPipeline();
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
                m_RenderCommandBuffer->EndTimerQuery(m_OpaqueForwardPass->GetName());
                EP_GPU_ZONE_END()
            }
        );
    }

    auto SceneRenderer::SkyPass() -> void
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
                EP_GPU_ZONE_END()
            }
        );
    }

    auto SceneRenderer::DisplayConversionPass() -> void
    {
        Renderer::Submit(
            [this]()
            {
                EP_PROFILE_FN("SceneRenderer::DisplayConversionPass")
                EP_GPU_ZONE(m_RenderCommandBuffer, "DisplayConversionPass")

                auto& statistics = m_DisplayConversionPass->GetStatistics();
                const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

                m_RenderCommandBuffer->BeginTimerQuery(m_DisplayConversionPass->GetName());
                m_RenderCommandBuffer->BeginMarker(m_DisplayConversionPass->GetName());
                Renderer::BeginRenderPass(m_RenderCommandBuffer, m_DisplayConversionPass);

                constexpr nvrhi::DrawArguments drawArgs{
                    .vertexCount = 3,
                    .instanceCount = 1,
                };
                cmdList->draw(drawArgs);

                statistics.DrawCalls++;
                statistics.Vertices += drawArgs.vertexCount;

                Renderer::EndRenderPass(m_RenderCommandBuffer);
                m_RenderCommandBuffer->EndMarker();
                m_RenderCommandBuffer->EndTimerQuery(m_DisplayConversionPass->GetName());
                EP_GPU_ZONE_END()
            }
        );
    }

    auto SceneRenderer::WireframePass() -> void
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
                    EP_GPU_ZONE_END()
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
                EP_GPU_ZONE_END()
            }
        );
    }

    auto SceneRenderer::EnsureIblResources() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EnsureIblResources")

        if (m_EnvironmentCube)
            return;

        m_EnvironmentCube = Image::Create(
            ImageSpecification{
                .ImageFormat = nvrhi::Format::RGBA16_FLOAT,
                .Width = s_IblEnvironmentSize,
                .Height = s_IblEnvironmentSize,
                .MipLevels = 1,
                .IsCubemap = true,
                .IsRenderTarget = true,
                .DebugName = "IBL Environment Cube",
            }
        );
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

        Ref<UniformBuffer> facesUB = Ref<UniformBuffer>::Create(sizeof(glm::mat4) * 6, "UniformBuffer IBL Faces");

        DeviceManager::Get()->GetDevice()->waitForIdle();

        Ref<RenderCommandBuffer> cmdBuffer = Ref<RenderCommandBuffer>::Create();
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

        cmdBuffer->End();
        cmdBuffer->Submit();
        DeviceManager::Get()->GetDevice()->waitForIdle();
    }

    auto SceneRenderer::RecordIblPass(
        const Ref<Shader>& shader, const Ref<Image>& source, const Ref<Sampler>& sampler, const Ref<Image>& target,
        const Ref<UniformBuffer>& facesUB, Ref<RenderCommandBuffer> cmdBuffer, const uint32_t mipLevel, const float roughness,
        const float envMapSize
    ) -> void
    {
        const auto framebuffer = Ref<Framebuffer>::Create(FramebufferSpecification{
            .Width = target->GetWidth(),
            .Height = target->GetHeight(),
            .Attachments = { FramebufferTextureSpecification(target) },
            .DebugName = "IBL Bake Framebuffer",
        });

        // FramebufferInfo carries only formats/samples, so mip zero's handle describes every mip; the pass selects the mip below.
        const auto pipeline = Ref<Pipeline>::Create(
            PipelineSpecification{
                .Shader = shader,
                .CullMode = nvrhi::RasterCullMode::None,
            },
            framebuffer->GetFramebuffer()->getFramebufferInfo()
        );

        Ref<RenderPass> pass = Ref<RenderPass>::Create(RenderPassSpecification{
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
