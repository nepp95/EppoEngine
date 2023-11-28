#pragma once

#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Eppo
{
	class Scene;
	using EntityHandle = entt::entity;

	// TODO: Move to renderer
	struct RenderSpecification
	{
		// Put things like quality level here
	};

	struct RenderStatistics
	{
		uint32_t DrawCalls = 0;
		uint32_t Meshes = 0;
	};

	class SceneRenderer
	{
	public:
		SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification);

		void RenderGui();

		void BeginScene(const EditorCamera& editorCamera);
		void EndScene();

		Ref<Image> GetFinalPassImage();

		void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, EntityHandle entityId);

	private:
		void Flush();

	private:
		RenderSpecification m_RenderSpecification;

		// Command buffer
		Ref<RenderCommandBuffer> m_CommandBuffer;

		// Pipelines
		Ref<Pipeline> m_GeometryPipeline;

		// Camera
		struct CameraData
		{
			glm::mat4 ViewProjection;
		} m_CameraBuffer;
		Ref<UniformBuffer> m_CameraUniformBuffer;

		// Draw commands
		struct DrawCommand
		{
			Ref<Mesh> Mesh;
			glm::mat4 Transform;
		};
		
		std::map<EntityHandle, DrawCommand> m_DrawList;

		// Statistics
		RenderStatistics m_RenderStatistics;

		struct TimestampQueries
		{
			uint32_t GeometryQuery = UINT32_MAX;
		} m_TimestampQueries;
	};
}
