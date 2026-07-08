#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderPass.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/VertexBuffer.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <nvrhi/nvrhi.h>

namespace Eppo
{
	class SceneRenderer
	{
	public:
		SceneRenderer(const Ref<Scene>& scene, uint32_t width = 0, uint32_t height = 0);

		auto RenderGui() const -> void;

		auto BeginScene(const ScopedPtr<EditorCamera>& camera) -> void;
		auto BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position) -> void;
		auto EndScene() -> void;

		auto GetFinalImage() const -> const Ref<Image>&;

		auto SubmitMesh(const AssetHandle meshHandle, const glm::mat4& transform) -> void;
		auto SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity) -> void;
		auto SubmitEnvironment(const EnvironmentSettings& environment) -> void;

		auto Resize(uint32_t width, uint32_t height) -> void;

	private:
		auto GeometryPass() -> void;
		auto SkyPass() -> void;

	private:
		Ref<Scene> m_Scene = nullptr;
		nvrhi::CommandListHandle m_CommandList = nullptr;

		RenderPass m_GeometryPass{ "Geometry" };
		RenderPass m_SkyPass{ "Sky" };

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		nvrhi::SamplerHandle m_Sampler = nullptr;

		Ref<Pipeline> m_GeometryPipeline = nullptr;
		Ref<Pipeline> m_SkyPipeline = nullptr;

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

		// Matches the geometry/skybox shaders' cbuffer layout. Each light is two
		// float4s (Position.xyz, Color.rgb + intensity in Color.a); NumLights caps
		// the shader loop so unused slots cost nothing.
		static constexpr uint32_t MaxPointLights = 32;
		struct LightData
		{
			struct PointLight
			{
				glm::vec4 Position{ 0.0f };
				glm::vec4 Color{ 0.0f }; // rgb = color, a = intensity
			};
			std::array<PointLight, MaxPointLights> Lights{};
			uint32_t NumLights = 0;
			float _pad[3]{};
		} m_LightData{};
		Ref<UniformBuffer> m_LightsUB = nullptr;
		bool m_LightOverflowWarned = false;

		// Mirrors the Environment cbuffer. Colors padded to float4; Params.x is the
		// ambient intensity, Params.y a 0/1 skybox flag (0 until an HDR loader lands).
		struct EnvironmentData
		{
			glm::vec4 ZenithColor{ 0.0f };
			glm::vec4 HorizonColor{ 0.0f };
			glm::vec4 GroundColor{ 0.0f };
			glm::vec4 Params{ 1.0f, 0.0f, 0.0f, 0.0f };
		} m_EnvironmentData{};
		Ref<UniformBuffer> m_EnvironmentUB = nullptr;
	};
}