#pragma once

#include "Renderer/Buffer/StorageBuffer.h"
#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Camera/SceneCamera.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Sampler.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

namespace Eppo
{
    struct SceneRendererSpecification
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        bool EnableDebugRendering = false;
    };

    class SceneRenderer
    {
    public:
        explicit SceneRenderer(const Ref<Scene>& scene, const SceneRendererSpecification& specification);

        auto RenderGui() const -> void;

        auto BeginScene(const EditorCamera& camera) -> void;
        auto BeginScene(const SceneCamera& camera, const glm::mat4& transform) -> void;
        auto EndScene() -> void;

        [[nodiscard]] auto GetFinalImage() const -> const Ref<Image>&;

        auto SubmitMesh(AssetHandle meshHandle, const glm::mat4& transform) -> void;
        auto SubmitEnvironmentSettings(const EnvironmentSettings& environment) -> void;

        auto Resize(uint32_t width, uint32_t height) -> void;

        auto SetDebugRenderingEnabled(const bool enabled) -> void { m_DebugRenderingEnabled = enabled; }
        [[nodiscard]] auto IsDebugRenderingEnabled() const -> bool { return m_DebugRenderingEnabled; }

        auto SetShowColliders(const bool enabled) -> void { m_ShowColliders = enabled; }
        [[nodiscard]] auto IsShowColliders() const -> bool { return m_ShowColliders; }

        auto SetShowWireframes(const bool enabled) -> void { m_ShowWireframes = enabled; }
        [[nodiscard]] auto IsShowWireframes() const -> bool { return m_ShowWireframes; }

        auto SetHighlightedEntity(const Entity entity) -> void { m_HighlightedEntity = entity; }

        // Keep the renderer's scene reference in sync with the editor's active scene.
        // m_Scene is set once at construction and goes stale across play/stop (the
        // editor swaps m_ActiveScene to a runtime copy); call this every frame.
        auto SetScene(const Ref<Scene>& scene) -> void { m_Scene = scene; }

    private:
        auto BeginSceneInternal() -> void;
        auto EnsureColliderMeshes() -> void;
        auto GatherWireframes() -> void;
        auto PrepareRenderData() -> void;
        auto UploadRenderData() -> void;

        auto SkyPass() const -> void;
        auto GeometryPass() -> void;
        auto WireframePass() const -> void;

    private:
        // Scene renderer settings
        Ref<RenderCommandBuffer> m_RenderCommandBuffer = nullptr;
        Ref<Scene> m_Scene = nullptr;

        bool m_DebugRenderingEnabled = false;
        bool m_ShowColliders = false;
        bool m_ShowWireframes = false;
        Entity m_HighlightedEntity;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        // Render passes
        Ref<RenderPass> m_SkyPass = nullptr;
        Ref<RenderPass> m_GeometryPass = nullptr;
        Ref<RenderPass> m_WireframePass = nullptr;

        // Extra pipelines
        Ref<Pipeline> m_GeometryDoubleSidedPipeline = nullptr;

        // Resources
        Ref<Mesh> m_BoxColliderMesh = nullptr;
        Ref<Mesh> m_SphereColliderMesh = nullptr;
        Ref<Mesh> m_CapsuleColliderMesh = nullptr;
        Ref<Mesh> m_CylinderColliderMesh = nullptr;

        // Uniforms
        struct CameraData
        {
            glm::mat4 View;
            glm::mat4 Projection;
            glm::mat4 ViewProjection;
            glm::mat4 InverseViewProjection;
            glm::vec4 Position;
            float NearClip;
            float FarClip;
        } m_CameraData{};
        Ref<UniformBuffer> m_CameraUB = nullptr;

        struct EnvironmentData
        {
            glm::vec4 ZenithColor = glm::vec4(0.0f);
            glm::vec4 HorizonColor = glm::vec4(0.0f);
            glm::vec4 GroundColor = glm::vec4(0.0f);
        } m_EnvironmentData{};
        Ref<UniformBuffer> m_EnvironmentUB = nullptr;

        // Draw commands
        struct DrawKey
        {
            UUID ID;

            auto operator<(const DrawKey& other) const -> bool { return ID < other.ID; }
        };

        struct DrawCommand
        {
            Ref<Mesh> Mesh = nullptr;
            std::vector<glm::mat4> Transforms;
            uint32_t InstanceOffset = 0;
            glm::vec4 Color = glm::vec4(1.0f);
        };

        std::map<DrawKey, DrawCommand> m_DrawCommands;
        std::vector<glm::mat4> m_InstanceTransforms;
        Ref<StorageBuffer> m_InstanceTransformsSB = nullptr;
        std::vector<glm::mat4> m_WireframeTransforms;
        std::vector<DrawCommand> m_WireframeDrawCommands;
        Ref<StorageBuffer> m_WireframeInstanceSB = nullptr;

        struct DrawData
        {
            glm::mat4 Transform; // 0-63
            uint32_t InstanceOffset; // 64-67
            uint32_t MaterialIndex; // 68-71
            uint32_t padding[2]; // 72 - 79
        };
        std::vector<DrawData> m_DrawData;
        Ref<StorageBuffer> m_DrawDataSB = nullptr;

        struct MaterialData
        {
            int32_t DiffuseMapIndex; // 0-3
            int32_t NormalMapIndex; // 4-7
            int32_t RoughMetMapIndex; // 8-11
            int32_t AOMapIndex; // 12-15
            int32_t EmissiveMapIndex; // 16-19
            uint32_t DiffuseSamplerIndex; // 20-23
            uint32_t NormalSamplerIndex; // 24-27
            uint32_t RoughMetSamplerIndex; // 28-31
            uint32_t AOSamplerIndex; // 32-35
            uint32_t EmissiveSamplerIndex; // 36-39
            uint32_t Padding0[2]; // 40-47
            glm::vec4 BaseColor; // 48-63
            glm::vec3 EmissiveFactor; // 64-75
            float Metallic; // 76-79
            float Roughness; // 80-83
            float NormalScale; // 84-87
            float AlphaCutoff; // 88-91
            uint32_t Flags; // 92-95 // bit 0+1 = alpha mode, bit 2 = double sided bool
        };
        std::vector<MaterialData> m_MaterialData;
        Ref<StorageBuffer> m_MaterialDataSB = nullptr;
    };
}
