#pragma once

#include "Renderer/Buffer/UniformBuffer.h"
#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
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

	enum class EnvironmentKeys
	{
		LightPosition,
		LightColor
	};

	class SceneRenderer
	{
	public:
		SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification);

		void RenderGui();

		void BeginScene(const EditorCamera& editorCamera);
		void EndScene();

		void SetEnvironment(EnvironmentKeys key, void* value);

		Ref<Image> GetFinalPassImage();

		void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, EntityHandle entityId);

	private:
		void Flush();

		void PrepareRender();

		void GeometryPass();
		void ShadowPass();

	private:
		RenderSpecification m_RenderSpecification;

		// Command buffer
		Ref<RenderCommandBuffer> m_CommandBuffer;

		struct CameraData
		{
			glm::mat4 View;
			glm::mat4 Projection;
			glm::mat4 ViewProjection;
		} m_CameraBuffer;
		Ref<UniformBuffer> m_CameraUniformBuffer;

		struct EnvironmentData
		{
			glm::mat4 LightView;
			glm::mat4 LightProjection;
			glm::mat4 LightViewProjection;
			glm::vec3 LightPosition = { 0.0f, 0.0f, 0.0f };
			glm::vec3 LightColor;
		} m_EnvironmentBuffer;
		Ref<UniformBuffer> m_EnvironmentUniformBuffer;

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
			uint32_t ShadowQuery = UINT32_MAX;
		} m_TimestampQueries;
	};
}
