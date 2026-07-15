#pragma once

#include "Renderer/Camera/Camera.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderPass.h"
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
		auto SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity) -> void;
		auto SubmitEnvironment(const EnvironmentSettings& environment) -> void;

		auto Resize(uint32_t width, uint32_t height) -> void;

		auto SetDebugRenderingEnabled(const bool enabled) -> void { m_DebugRenderingEnabled = enabled; }
		[[nodiscard]] auto IsDebugRenderingEnabled() const -> bool { return m_DebugRenderingEnabled; }

		// Colliders are a debug sub-feature: only drawn when debug rendering is on,
		// but the toggle is persisted on the renderer so re-enabling debug rendering
		// remembers whether colliders should come back.
		auto SetShowColliders(const bool enabled) -> void { m_ShowColliders = enabled; }
		[[nodiscard]] auto IsShowColliders() const -> bool { return m_ShowColliders; }

		// Mesh wireframes are a debug sub-feature: overlays every entity's mesh as
		// a wireframe outline when debug rendering is on. Same persistence pattern
		// as ShowColliders.
		auto SetShowWireframes(const bool enabled) -> void { m_ShowWireframes = enabled; }
		[[nodiscard]] auto IsShowWireframes() const -> bool { return m_ShowWireframes; }

		auto SetHighlightedEntity(const Entity entity) -> void { m_HighlightedEntity = entity; }

		// Keep the renderer's scene reference in sync with the editor's active scene.
		// m_Scene is set once at construction and goes stale across play/stop (the
		// editor swaps m_ActiveScene to a runtime copy); call this every frame.
		auto SetScene(const Ref<Scene>& scene) -> void { m_Scene = scene; }

	private:
        auto BeginSceneInternal() -> void;

		auto GeometryPass() -> void;
		auto SkyPass() -> void;
		auto WireframePass() -> void;

	private:
		Ref<Scene> m_Scene = nullptr;
		nvrhi::CommandListHandle m_CommandList = nullptr;

		bool m_DebugRenderingEnabled = false;
		bool m_ShowColliders = true;
		bool m_ShowWireframes = true;
		Entity m_HighlightedEntity;

		// Dimensions precede the passes: the passes are constructed in the
		// member initializer list and their pipelines derive from m_Width/m_Height.
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		RenderPass m_GeometryPass;
		RenderPass m_SkyPass;
		RenderPass m_WireframePass;

		nvrhi::SamplerHandle m_Sampler = nullptr;

		struct DrawKey
		{
			UUID ID;

			bool operator<(const DrawKey& other) const
			{
				return ID < other.ID;
			}
		};

		struct DrawCommand
		{
			Ref<Mesh> Mesh = nullptr;
			std::vector<glm::mat4> Transforms = { glm::mat4(1.0f) };
			uint32_t ImageCount = 0;
			uint32_t ImageOffset = 0;
			uint32_t InstanceOffset = 0;
		};
		std::map<DrawKey, DrawCommand> m_DrawCommands;
		Ref<StorageBuffer> m_InstanceTransformsSB = nullptr;

		// WireframePass owns its own instance buffer (collider/highlight draws are
		// separate from geometry instances). Collider primitive meshes are fetched
		// from the AssetManager on demand, so they share its lazy cache.
		Ref<StorageBuffer> m_WireframeInstanceSB = nullptr;

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
			struct PointLight
			{
				glm::vec4 Position = glm::vec4(1.0f);
				glm::vec4 Color = glm::vec4(1.0f); // rgb = color, a = intensity
			};

			std::array<PointLight, MaxPointLights> Lights{};
			uint32_t NumLights = 0;
		} m_LightData{};
		Ref<UniformBuffer> m_LightsUB = nullptr;

		struct EnvironmentData
		{
            glm::vec4 ZenithColor = glm::vec4(0.0f);
            glm::vec4 HorizonColor = glm::vec4(0.0f);
            glm::vec4 GroundColor = glm::vec4(0.0f);
            glm::vec4 Params = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		} m_EnvironmentData{};
		Ref<UniformBuffer> m_EnvironmentUB = nullptr;
	};
}