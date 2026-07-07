#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
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

		auto Resize(uint32_t width, uint32_t height) -> void;

	private:
		auto GeometryPass() -> void;

	private:
		Ref<Scene> m_Scene = nullptr;
		nvrhi::CommandListHandle m_CommandList = nullptr;
		std::vector<nvrhi::TimerQueryHandle> m_TimerQueries;
		std::vector<float> m_LastQueryTimes;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		nvrhi::SamplerHandle m_Sampler = nullptr;

		Ref<Pipeline> m_GeometryPipeline = nullptr;

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
		} m_CameraData{};
		Ref<UniformBuffer> m_CameraUB = nullptr;

		struct LightData
		{
			glm::vec3 Position;
			float Fill;
			glm::vec4 Color;
		};
		std::array<LightData, 4> m_LightData{};
		Ref<UniformBuffer> m_LightsUB = nullptr;

		struct DrawStatistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Submeshes = 0;
			uint32_t Instances = 0;
		} m_DrawStatistics;
	};
}