#pragma once

#include "Core/Buffer.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/DebugRenderer.h"
#include "Renderer/Image.h"
#include "Renderer/Pipeline.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/UniformBuffer.h"

namespace Eppo
{
	class VulkanSceneRenderer : public SceneRenderer
	{
	public:
		VulkanSceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpec);
		virtual ~VulkanSceneRenderer() = default;

		void RenderGui() override;
		void Resize(uint32_t width, uint32_t height) override;
		
		void BeginScene(const EditorCamera& editorCamera) override;
		void BeginScene(const Camera& camera, const glm::mat4& transform) override;
		void EndScene() override;
		
		void SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId) override;
		Ref<Image> GetFinalImage() override;

	private:
		void BeginSceneInternal();

		void Flush();
		void PrepareRender() const;

		void PreDepthPass();
		void GeometryPass();
		void DebugLinePass();
		void CompositePass();

	private:
		RenderSpecification m_RenderSpecification;
		Ref<Scene> m_Scene;

		Ref<CommandBuffer> m_CommandBuffer;
		Ref<DebugRenderer> m_DebugRenderer;

		Ref<Pipeline> m_PreDepthPipeline;
		Ref<Pipeline> m_GeometryPipeline;
		Ref<Pipeline> m_DebugLinePipeline;
		Ref<Pipeline> m_CompositePipeline;

		static const uint32_t s_MaxLights = 8;

		// Set 1, Binding 0
		struct CameraData
		{
			glm::mat4 View;
			glm::mat4 Projection;
			glm::mat4 ViewProjection;
			glm::vec4 Position;
		} m_CameraBuffer;
		Ref<UniformBuffer> m_CameraUB;

		// Set 1, Binding 1
		struct Light
		{
			glm::mat4 View[6];
			glm::vec4 Position = glm::vec4(0.0f);
			glm::vec4 Color = glm::vec4(0.0f);
		};

		struct LightsData
		{
			glm::mat4 Projection;
			Light Lights[s_MaxLights];
			uint32_t NumLights;
		} m_LightsBuffer;
		Ref<UniformBuffer> m_LightsUB;

		// Set 1, Binding 2
		std::array<Ref<Image>, s_MaxLights> m_ShadowMaps;

		// Draw commands
		struct DrawCommand
		{
			EntityHandle Handle;
			Ref<Mesh> Mesh;
			glm::mat4 Transform;
		};

		std::vector<DrawCommand> m_DrawList;

		// Buffers
		Buffer m_PushConstantBuffer;
		Ref<VertexBuffer> m_DebugLineVertexBuffer;
		Ref<IndexBuffer> m_DebugLineIndexBuffer;

		// Statistics
		RenderStatistics m_RenderStatistics;

		struct TimestampQueries
		{
			uint32_t PreDepthQuery = UINT32_MAX;
			uint32_t GeometryQuery = UINT32_MAX;
			uint32_t DebugLineQuery = UINT32_MAX;
			uint32_t CompositeQuery = UINT32_MAX;
		} m_TimestampQueries;
	};
}
