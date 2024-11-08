#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/DescriptorWriter.h"
#include "Renderer/Image.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Scene/Components.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Eppo
{
	class Scene;
	using EntityHandle = entt::entity;

	// TODO: Move to renderer
	struct RenderSpecification
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
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

		void Resize(uint32_t width, uint32_t height);

		void BeginScene(const EditorCamera& editorCamera);
		void BeginScene(const Camera& camera, const glm::mat4& transform);
		void EndScene();

		void SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId);

		Ref<Image> GetFinalImage() const;

	private:
		void Flush();

		void PrepareRender();

		void PreDepthPass();
		void GeometryPass();
		void CompositePass();

	private:
		RenderSpecification m_RenderSpecification;
		Ref<Scene> m_Scene;

		Ref<RenderCommandBuffer> m_CommandBuffer;

		Ref<Pipeline> m_PreDepthPipeline;
		Ref<Pipeline> m_GeometryPipeline;
		Ref<Pipeline> m_CompositePipeline;

		uint32_t Width = 0;
		uint32_t Height = 0;

		// Binding 0
		struct CameraData
		{
			glm::mat4 View;
			glm::mat4 Projection;
			glm::mat4 ViewProjection;
			glm::vec4 Position;
		} m_CameraBuffer;
		Ref<UniformBuffer> m_CameraUB;

		// Binding 1
		struct Light
		{
			glm::mat4 View[6];
			glm::vec4 Position = glm::vec4(0.0f);
			glm::vec4 Color = glm::vec4(0.0f);
		};

		struct LightsData
		{
			glm::mat4 Projection;
			Light Lights[8];
			uint32_t NumLights;
		} m_LightsBuffer;
		Ref<UniformBuffer> m_LightsUB;

		// Draw commands
		struct DrawCommand
		{
			EntityHandle Handle;
			Ref<Mesh> Mesh;
			glm::mat4 Transform;
		};
		
		std::vector<DrawCommand> m_DrawList;

		// Statistics
		RenderStatistics m_RenderStatistics;
	};
}
