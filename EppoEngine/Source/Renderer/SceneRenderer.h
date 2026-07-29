#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Camera/SceneCamera.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Sampler.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <nvrhi/nvrhi.h>

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
        auto SubmitEnvironment(const EnvironmentSettings& environment) -> void;

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
        auto PrepareRender() -> void;
        auto FillShadowData() -> void;

        auto ShadowDepthPass() -> void;
        auto GeometryPass() -> void;
        auto SkyPass() const -> void;
        auto TonemapPass() const -> void;
        auto WireframePass() const -> void;

        auto EnsureIblResources() -> void;
        auto BakeEnvironmentMap(const Ref<Image>& equirect) -> void;
        auto RecordIblPass(
            const Ref<Shader>& shader, const Ref<Image>& source, const Ref<Sampler>& sampler, const Ref<Image>& target,
            const Ref<UniformBuffer>& facesUB, const Ref<RenderCommandBuffer>& cmdBuffer, uint32_t mipLevel, uint32_t size,
            float roughness = 0.0f, float envMapSize = 0.0f
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
        Ref<RenderPass> m_GeometryPass = nullptr;
        Ref<RenderPass> m_SkyPass = nullptr;
        Ref<RenderPass> m_TonemapPass = nullptr;
        Ref<RenderPass> m_WireframePass = nullptr;

        // Resources
        Ref<Mesh> m_BoxColliderMesh = nullptr;
        Ref<Mesh> m_SphereColliderMesh = nullptr;
        Ref<Mesh> m_CapsuleColliderMesh = nullptr;
        Ref<Mesh> m_CylinderColliderMesh = nullptr;

        Ref<Sampler> m_ClampAllFiltersFalseSampler = nullptr;
        Ref<Sampler> m_ClampAllFiltersTrueSampler = nullptr;
        Ref<Sampler> m_WrapAllFiltersTrueSampler = nullptr;
        Ref<Sampler> m_EquirectSampler = nullptr;

        // Uniforms
        struct ShadowDepthData
        {
            glm::mat4 LightViewProjection;
            glm::uvec4 Indices; // shadow map, sampler, enabled, unused
            glm::vec4 Params; // bias, inverse map size, unused, unused
        } m_ShadowDepthData;
        Ref<UniformBuffer> m_ShadowDepthUB = nullptr;

        struct CameraData
        {
            glm::mat4 View;
            glm::mat4 Projection;
            glm::mat4 ViewProjection;
            glm::vec4 Position;
            glm::mat4 InverseViewProjection;
        } m_CameraData{};
        Ref<UniformBuffer> m_CameraUB = nullptr;

        static constexpr uint32_t MaxPointLights = 32;
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
            glm::vec4 Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // x = ambient intensity, y = has skybox
            glm::uvec4 IBL0 = glm::uvec4(0); // x = env cube, y = irradiance, z = prefilter, w = BRDF LUT bindless indices
            glm::uvec4 IBL1 = glm::uvec4(0); // x = IBL sampler index (clamp — cube/LUT edge taps must not wrap)
        } m_EnvironmentData{};
        Ref<UniformBuffer> m_EnvironmentUB = nullptr;

        Ref<Image> m_EnvironmentCube = nullptr;
        Ref<Image> m_IrradianceCube = nullptr;
        Ref<Image> m_PrefilterCube = nullptr;
        Ref<Image> m_BrdfLut = nullptr;

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
        Ref<StorageBuffer> m_InstanceTransformsSB = nullptr;
        std::vector<DrawCommand> m_WireframeDrawCommands;
        Ref<StorageBuffer> m_WireframeInstanceSB = nullptr;
    };
}
