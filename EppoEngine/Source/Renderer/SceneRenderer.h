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

		// Submit debug primitives through the getter between BeginScene and EndScene.
        auto SetDebugRenderingEnabled(const bool enabled) -> void { m_DebugRenderingEnabled = enabled; }
        [[nodiscard]] auto IsDebugRenderingEnabled() const -> bool { return m_DebugRenderingEnabled; }

	private:
        auto BeginSceneInternal() -> void;

		auto GeometryPass() -> void;
		auto SkyPass() -> void;
		auto WireframePass() -> void;

	private:
		Ref<Scene> m_Scene = nullptr;
		nvrhi::CommandListHandle m_CommandList = nullptr;

		bool m_DebugRenderingEnabled = false;
		Entity m_HighlightedEntity;

		RenderPass m_GeometryPass{ "Geometry" };
		RenderPass m_SkyPass{ "Sky" };
	    RenderPass m_WireframePass{ "Wireframe" };

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		nvrhi::SamplerHandle m_Sampler = nullptr;

		Ref<Pipeline> m_GeometryPipeline = nullptr;
		Ref<Pipeline> m_SkyPipeline = nullptr;
	    Ref<Pipeline> m_WireframePipeline = nullptr;

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
			float _pad[3]{};
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