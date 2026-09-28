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

        // Geometry (forward opaque)
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
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

        // Skybox
        {
            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("skybox"),
                .CullMode = nvrhi::RasterCullMode::None,
            };

            const RenderPassSpecification renderPassSpec{
                .Name = "Skybox",
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_GeometryPass->GetFramebuffer()->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_GeometryPass->GetFramebuffer(),
                .OwnsFramebuffer = false,
            };

            m_SkyPass = CreateRef<RenderPass>(renderPassSpec);
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
                .Pipeline = CreateRef<Pipeline>(pipelineSpec, m_GeometryPass->GetFramebuffer()->GetFramebuffer()->getFramebufferInfo()),
                .Framebuffer = m_GeometryPass->GetFramebuffer(),
                .OwnsFramebuffer = false,
            };

            m_WireframePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Uniform buffers
        m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Camera");
        m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), "UniformBuffer Environment");

        m_InstanceTransformsSB = CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Instance Transforms");
        m_WireframeInstanceSB =
            CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Wireframe Instance Transforms");
        m_DrawDataSB = CreateRef<StorageBuffer>(sizeof(DrawData), sizeof(DrawData), "StorageBuffer Draw Data");
        m_MaterialDataSB = CreateRef<StorageBuffer>(sizeof(MaterialData), sizeof(MaterialData), "StorageBuffer Material Data");

        // Inputs retain their resources and resolve current GPU handles whenever a pass bakes.
        m_GeometryPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_GeometryPass->SetInput(0, 1, m_DrawDataSB);
        m_GeometryPass->SetInput(0, 2, m_MaterialDataSB);
        m_GeometryPass->SetInput(0, 2, m_CameraUB);

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
            { .Pass = m_GeometryPass },
            { .Pass = m_SkyPass },
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
        GeometryPass();
        SkyPass();
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
        return m_GeometryPass->GetFramebuffer()->GetFinalImage();
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

    auto SceneRenderer::SubmitEnvironmentSettings(const EnvironmentSettings& environment) -> void
    {
        m_EnvironmentData.ZenithColor = glm::vec4(environment.ZenithColor, 1.0f);
        m_EnvironmentData.HorizonColor = glm::vec4(environment.HorizonColor, 1.0f);
        m_EnvironmentData.GroundColor = glm::vec4(environment.GroundColor, 1.0f);
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
        m_WireframePass->Resize(m_Width, m_Height);
    }

    auto SceneRenderer::BeginSceneInternal() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginSceneInternal");

        Renderer::Submit(
            [this]()
            {
                std::memset(&m_GeometryPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_SkyPass->GetStatistics(), 0, sizeof(PassStatistics));
                std::memset(&m_WireframePass->GetStatistics(), 0, sizeof(PassStatistics));
            }
        );

        m_DrawCommands.clear();

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

        if (!markerDraw.Transforms.empty())
        {
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

                // Framebuffer attachments are recreated on resize.
                m_SkyPass->SetInput(0, 0, m_GeometryPass->GetFramebuffer()->GetDepthImage());
                m_WireframePass->SetInput(0, 2, m_GeometryPass->GetFramebuffer()->GetDepthImage());

                // Rebuild binding sets for any pass whose resource handles changed.
                m_GeometryPass->Bake();
                m_SkyPass->Bake();
                m_WireframePass->Bake();
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
                EP_GPU_ZONE_END()
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
                EP_GPU_ZONE_END()
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
}
