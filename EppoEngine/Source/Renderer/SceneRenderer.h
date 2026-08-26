#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Camera/SceneCamera.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Sampler.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"
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
        auto SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity) -> void;
        auto SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity) -> void;
        // TODO: Remove these 3. We have the scene, the scene has all these.
        auto SubmitEnvironmentSettings(const EnvironmentSettings& environment) -> void;
        auto SubmitBloomSettings(const BloomSettings& bloom) -> void;
        auto SubmitSsaoSettings(const SsaoSettings& ssao) -> void;

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
        auto FillShadowData() -> void;

        auto ShadowDepthPass() -> void;
        auto SsaoPass() -> void;
        auto GeometryPass() -> void;
        auto LightingPass() -> void;
        auto SkyPass() const -> void;
        auto BloomPass() -> void;
        auto TonemapPass() const -> void;
        auto WireframePass() const -> void;

        auto EnsureIblResources() -> void;
        auto BakeEnvironmentMap(const Ref<Image>& equirect) -> void;
        auto RecordEnvironmentMipPass(
            const Ref<Image>& target, const Ref<UniformBuffer>& facesUB, const Ref<RenderCommandBuffer>& cmdBuffer, uint32_t mipLevel
        ) const -> void;
        auto RecordIblPass(
            const Ref<Shader>& shader, const Ref<Image>& source, const Ref<Sampler>& sampler, const Ref<Image>& target,
            const Ref<UniformBuffer>& facesUB, const Ref<RenderCommandBuffer>& cmdBuffer, uint32_t mipLevel, float roughness = 0.0f,
            float envMapSize = 0.0f
        ) -> void;

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
        Ref<RenderPass> m_ShadowDepthPass = nullptr;
        Ref<RenderPass> m_SsaoEvaluationPass = nullptr;
        Ref<RenderPass> m_SsaoBlurHorizontalPass = nullptr;
        Ref<RenderPass> m_GeometryPass = nullptr;
        Ref<RenderPass> m_LightingPass = nullptr;
        Ref<RenderPass> m_SkyPass = nullptr;

        uint32_t m_BloomMipLevels = 0;
        Ref<Framebuffer> m_BloomPyramidFramebuffer = nullptr;
        Ref<RenderPass> m_BloomDownSamplePass = nullptr;
        Ref<RenderPass> m_BloomUpSamplePass = nullptr;

        Ref<RenderPass> m_TonemapPass = nullptr;
        Ref<RenderPass> m_WireframePass = nullptr;

        // Extra pipelines
        Ref<Pipeline> m_GeometryDoubleSidedPipeline = nullptr;
        Ref<Pipeline> m_ShadowDepthDoubleSidedPipeline = nullptr;

        // Resources
        Ref<Mesh> m_BoxColliderMesh = nullptr;
        Ref<Mesh> m_SphereColliderMesh = nullptr;
        Ref<Mesh> m_CapsuleColliderMesh = nullptr;
        Ref<Mesh> m_CylinderColliderMesh = nullptr;

        // Uniforms
        static constexpr uint32_t s_ShadowCascadeCount = 4;
        struct Cascade
        {
            glm::mat4 LightViewProjection;
            float SplitDistance; // view-space far depth of this cascade
            float WorldUnitsPerTexel;
            float TransitionStart;
            float Padding;
        };
        struct ShadowDepthData
        {
            std::array<Cascade, s_ShadowCascadeCount> Cascades;
            uint32_t ShadowMapIndex;
            uint32_t ShadowSamplerIndex;
            float DepthBiasTexels;
            float NormalBiasTexels;
            float InvMapSize;
            float ShadowDistance;
        } m_ShadowDepthData;
        Ref<UniformBuffer> m_ShadowDepthUB = nullptr;

        static constexpr uint32_t s_SsaoKernelSize = 32;
        struct SsaoData
        {
            std::array<glm::vec4, s_SsaoKernelSize> Kernel;
            glm::vec4 Params;
            glm::vec4 InvSize;
        } m_SsaoData;
        Ref<UniformBuffer> m_SsaoUB = nullptr;
        SsaoSettings m_SsaoSettings;

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

        static constexpr uint32_t MaxPointLights = 16;
        struct LightData
        {
            struct DirectionalLight
            {
                glm::vec4 Direction;
                glm::vec4 Color;
            };

            struct PointLight
            {
                glm::vec4 Position = glm::vec4(1.0f);
                glm::vec4 Color = glm::vec4(1.0f); // rgb = color, a = intensity
            };

            DirectionalLight DirectionalLight{};
            std::array<PointLight, MaxPointLights> Lights{};
            uint32_t NumLights = 0;
            uint32_t HasDirectionalLight = 0;
        } m_LightData{};
        Ref<UniformBuffer> m_LightsUB = nullptr;

        struct EnvironmentData
        {
            glm::vec4 ZenithColor = glm::vec4(0.0f);
            glm::vec4 HorizonColor = glm::vec4(0.0f);
            glm::vec4 GroundColor = glm::vec4(0.0f);
            glm::vec4 Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // x = ambient intensity, y = has skybox, z = exposure
            glm::uvec4 IBL0 = glm::uvec4(0); // x = env cube, y = irradiance, z = prefilter, w = BRDF LUT bindless indices
            glm::uvec4 IBL1 = glm::uvec4(0); // x = IBL sampler index (clamp — cube/LUT edge taps must not wrap)
        } m_EnvironmentData{};
        Ref<UniformBuffer> m_EnvironmentUB = nullptr;

        Ref<Image> m_EnvironmentCube = nullptr;
        Ref<Image> m_IrradianceCube = nullptr;
        Ref<Image> m_PrefilterCube = nullptr;
        Ref<Image> m_BrdfLut = nullptr;

        BloomSettings m_BloomSettings;

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
