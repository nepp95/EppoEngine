#include "pch.h"
#include "Renderer/SceneRenderer.h"

#include "Core/Application.h"
#include "Project/Project.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nvrhi/utils.h>

#include <ranges>

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

        m_Sampler = Sampler::Create();
        m_RenderCommandBuffer = CreateRef<RenderCommandBuffer>();

        // Create render passes
        // Geometry
        {
            const FramebufferSpecification framebufferSpec{
                .Width = m_Width,
                .Height = m_Height,
                .Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
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

            auto pipeline = CreateRef<Pipeline>(pipelineSpec);

            const RenderPassSpecification renderPassSpec{
                .Name = "Geometry",
                .Pipeline = pipeline,
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

            auto pipeline = CreateRef<Pipeline>(pipelineSpec);

            const RenderPassSpecification renderPassSpec{
                .Name = "Skybox",
                .Pipeline = pipeline,
            };

            m_SkyPass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Wireframes
        {
            const PipelineSpecification pipelineSpec{
                .Shader = renderer->GetShader("wireframe"),
                .Framebuffer = m_GeometryPass->GetPipeline()->GetSpecification().Framebuffer,
                .OwnsFramebuffer = false,
                .Width = m_Width,
                .Height = m_Height,
                .CullMode = nvrhi::RasterCullMode::None,
                .FillMode = nvrhi::RasterFillMode::Wireframe,
                .DepthTestEnable = true,
                .DepthWriteEnable = false,
                .DepthFunc = nvrhi::ComparisonFunc::LessOrEqual,
                .DepthBias = -1,
                .SlopeScaledDepthBias = -1.0f,
            };

            auto pipeline = CreateRef<Pipeline>(pipelineSpec);

            const RenderPassSpecification renderPassSpec{
                .Name = "Wireframes",
                .Pipeline = pipeline,
            };

            m_WireframePass = CreateRef<RenderPass>(renderPassSpec);
        }

        // Uniform buffers
        m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Camera");
        m_LightsUB = CreateRef<UniformBuffer>(sizeof(LightData), "UniformBuffer Lights");
        m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), "UniformBuffer Environment");

        m_InstanceTransformsSB =
            CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Instance Transforms");
        m_WireframeInstanceSB =
            CreateRef<StorageBuffer>(sizeof(glm::mat4), sizeof(glm::mat4), "StorageBuffer Wireframe Instance Transforms");

        // Inputs retain their resources and resolve current GPU handles whenever a pass bakes.
        m_GeometryPass->SetInput(0, 0, m_InstanceTransformsSB);
        m_GeometryPass->SetInput(0, 1, m_CameraUB);
        m_GeometryPass->SetInput(0, 2, m_LightsUB);
        m_GeometryPass->SetInput(0, 3, m_EnvironmentUB);

        m_SkyPass->SetInput(0, 1, m_CameraUB);
        m_SkyPass->SetInput(0, 3, m_EnvironmentUB);

        m_WireframePass->SetInput(0, 0, m_WireframeInstanceSB);
        m_WireframePass->SetInput(0, 1, m_CameraUB);
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
            if (!ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen, "%s: %.2fms", name, timeMs))
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
        const auto& geometryStats = m_GeometryPass->GetStatistics();
        const auto& skyStats = m_SkyPass->GetStatistics();
        const auto& wireframeStats = m_WireframePass->GetStatistics();
        renderPass(
            m_GeometryPass->GetName().c_str(), geometryStats, m_RenderCommandBuffer->GetTimeMs(m_GeometryPass->GetName(), frameIndex)
        );
        renderPass(m_SkyPass->GetName().c_str(), skyStats, m_RenderCommandBuffer->GetTimeMs(m_SkyPass->GetName(), frameIndex));
        renderPass(
            m_WireframePass->GetName().c_str(), wireframeStats, m_RenderCommandBuffer->GetTimeMs(m_WireframePass->GetName(), frameIndex)
        );

        PassStatistics sceneStats;
        sceneStats += geometryStats;
        sceneStats += skyStats;
        sceneStats += wireframeStats;
        const float sceneTime = m_RenderCommandBuffer->GetTimeMs(m_GeometryPass->GetName(), frameIndex) +
            m_RenderCommandBuffer->GetTimeMs(m_SkyPass->GetName(), frameIndex) +
            m_RenderCommandBuffer->GetTimeMs(m_WireframePass->GetName(), frameIndex);
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

    auto SceneRenderer::BeginSceneInternal() -> void
    {
        EP_PROFILE_FN("SceneRenderer::BeginSceneInternal")

        std::memset(&m_GeometryPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_SkyPass->GetStatistics(), 0, sizeof(PassStatistics));
        std::memset(&m_WireframePass->GetStatistics(), 0, sizeof(PassStatistics));

        m_DrawCommands.clear();
        m_LightData.NumLights = 0;

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

        m_Wireframes.clear();
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
            m_Scene->ForEachEntity(
                [&](const Entity entity)
                -> void {
                    const glm::mat4 world = m_Scene->GetWorldTransform(entity);

                    if (entity.HasComponent<BoxColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<BoxColliderComponent>();
                        m_Wireframes.push_back({ m_BoxColliderMesh, glm::scale(glm::translate(world, c.Offset), c.HalfSize), colliderColor });
                    }

                    if (entity.HasComponent<SphereColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<SphereColliderComponent>();
                        m_Wireframes.push_back(
                            { m_SphereColliderMesh, glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius)), colliderColor }
                        );
                    }

                    if (entity.HasComponent<CapsuleColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<CapsuleColliderComponent>();
                        m_Wireframes.push_back(
                            { m_CapsuleColliderMesh,
                              glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius, c.Height / 2.0f, c.Radius)), colliderColor }
                        );
                    }

                    if (entity.HasComponent<CylinderColliderComponent>())
                    {
                        const auto& c = entity.GetComponent<CylinderColliderComponent>();
                        m_Wireframes.push_back(
                            { m_CylinderColliderMesh,
                              glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius, c.Height / 2.0f, c.Radius)), colliderColor }
                        );
                    }
                }
            );
        }

        if (m_ShowWireframes)
        {
            m_Scene->ForEachEntity(
                [&](Entity entity)
                {
                    if (!entity.HasComponent<MeshComponent>())
                        return;

                    if (const auto& mc = entity.GetComponent<MeshComponent>(); mc.MeshHandle)
                    {
                        const auto mesh = assetManager->GetOrLoadAsset<Mesh>(mc.MeshHandle);
                        const glm::mat4 world = m_Scene->GetWorldTransform(entity);
                        m_Wireframes.push_back({ mesh, world, meshWireframeColor });
                    }
                }
            );
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
                    m_Wireframes.push_back({ m_BoxColliderMesh, boxTransform, highlightColor });
                }
            }
        }
    }

    auto SceneRenderer::PrepareRender() -> void
    {
        EP_PROFILE_FN("SceneRenderer::PrepareRender")

        const auto& cmdList = m_RenderCommandBuffer->GetCommandList();

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

        // Gather collider, mesh-overlay and selection wireframes.
        GatherWireframes();

        // Upload one instance transform per gathered wireframe draw.
        std::vector<glm::mat4> wireframeTransforms;
        wireframeTransforms.reserve(m_Wireframes.size());
        for (const auto& draw : m_Wireframes)
            wireframeTransforms.push_back(draw.Transform);

        const uint64_t wireframeSize = wireframeTransforms.size() * sizeof(glm::mat4);
        m_WireframeInstanceSB->SetData(cmdList, wireframeTransforms.data(), wireframeSize);

        // Rebuild binding sets for any pass whose resource handles changed.
        m_GeometryPass->Bake();
        m_SkyPass->Bake();
        m_WireframePass->Bake();
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
        // Params.y is the skybox flag; kept 0 until an HDR sky loader lands.
        m_EnvironmentData.Params = glm::vec4(environment.AmbientIntensity, 0.0f, 0.0f, 0.0f);
    }

    auto SceneRenderer::EndScene() -> void
    {
        EP_PROFILE_FN("SceneRenderer::EndScene")

        EnsureColliderMeshes();

        m_RenderCommandBuffer->Begin();
        PrepareRender();

        GeometryPass();
        SkyPass();
        WireframePass();

        m_RenderCommandBuffer->End();
        m_RenderCommandBuffer->Submit();
    }

    auto SceneRenderer::GetFinalImage() const -> const Ref<Image>&
    {
        return m_GeometryPass->GetPipeline()->GetSpecification().Framebuffer->GetFinalImage();
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

    auto SceneRenderer::GeometryPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::GeometryPass")

        GeometryPushConstants pushConstants{};

        const auto& renderer = DeviceManager::Get()->GetRenderer();
        auto& statistics = m_GeometryPass->GetStatistics();

        m_RenderCommandBuffer->BeginTimerQuery(m_GeometryPass->GetName());
        renderer->BeginRenderPass(m_RenderCommandBuffer, m_GeometryPass);

        auto& state = m_RenderCommandBuffer->GetGraphicsState();
        pushConstants.SamplerIndex = m_Sampler->GetBindlessIndex();

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
                    pushConstants.BaseColor = material->BaseColor;
                    pushConstants.DiffuseMapIndex = material->GetDiffuseMapIndex();
                    pushConstants.NormalMapIndex = material->GetNormalMapIndex();
                    pushConstants.RoughMetMapIndex = material->GetRoughMetMapIndex();
                    pushConstants.Metallic = material->Metallic;
                    pushConstants.Roughness = material->Roughness;
                    m_RenderCommandBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(GeometryPushConstants));

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

        renderer->EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndTimerQuery(m_GeometryPass->GetName());
    }

    auto SceneRenderer::SkyPass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::SkyPass")

        const auto& renderer = DeviceManager::Get()->GetRenderer();
        auto& statistics = m_SkyPass->GetStatistics();
        m_RenderCommandBuffer->BeginTimerQuery(m_SkyPass->GetName());
        renderer->BeginRenderPass(m_RenderCommandBuffer, m_SkyPass);

        constexpr nvrhi::DrawArguments drawArgs{
            .vertexCount = 3,
            .instanceCount = 1,
        };
        m_RenderCommandBuffer->GetCommandList()->draw(drawArgs);

        statistics.DrawCalls++;
        statistics.Vertices += drawArgs.vertexCount;

        renderer->EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndTimerQuery(m_SkyPass->GetName());
    }

    auto SceneRenderer::WireframePass() -> void
    {
        EP_PROFILE_FN("SceneRenderer::WireframePass")

        if (!m_DebugRenderingEnabled)
        {
            m_RenderCommandBuffer->BeginTimerQuery(m_WireframePass->GetName());
            m_RenderCommandBuffer->EndTimerQuery(m_WireframePass->GetName());
            return;
        }

        if (m_Wireframes.empty())
        {
            m_RenderCommandBuffer->BeginTimerQuery(m_WireframePass->GetName());
            m_RenderCommandBuffer->EndTimerQuery(m_WireframePass->GetName());
            return;
        }

        WireframePushConstants pushConstants{};

        const auto& renderer = DeviceManager::Get()->GetRenderer();
        auto& statistics = m_WireframePass->GetStatistics();
        m_RenderCommandBuffer->BeginTimerQuery(m_WireframePass->GetName());
        renderer->BeginRenderPass(m_RenderCommandBuffer, m_WireframePass);
        auto& state = m_RenderCommandBuffer->GetGraphicsState();

        for (uint32_t drawIndex = 0; const auto& draw : m_Wireframes)
        {
            for (const auto& submesh : draw.Mesh->GetSubmeshes())
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
                pushConstants.Color = draw.Color;
                pushConstants.InstanceOffset = drawIndex;

                for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
                {
                    m_RenderCommandBuffer->GetCommandList()->setPushConstants(&pushConstants, sizeof(WireframePushConstants));

                    nvrhi::DrawArguments drawArgs{
                        .vertexCount = static_cast<uint32_t>(indexCount),
                        .instanceCount = 1,
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
            statistics.Instances++;
            ++drawIndex;
        }

        renderer->EndRenderPass(m_RenderCommandBuffer);
        m_RenderCommandBuffer->EndTimerQuery(m_WireframePass->GetName());
    }
}
