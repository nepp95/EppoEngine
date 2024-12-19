#pragma once

#include "Core/Buffer.h"
#include "Platform/Vulkan/Vulkan.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/DrawCommand.h"
#include "Renderer/DebugRenderer.h"
#include "Renderer/Image.h"
#include "Renderer/Pipeline.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/RenderTypes.h"

namespace Eppo
{
	class VulkanSceneRenderer : public SceneRenderer
	{
	public:
		VulkanSceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpec);
		~VulkanSceneRenderer() override = default;

		void RenderGui() override;
		void Resize(uint32_t width, uint32_t height) override;
		
		void BeginScene(const EditorCamera& editorCamera) override;
		void BeginScene(const Camera& camera, const glm::mat4& transform) override;
		void EndScene() override;
		
		void SubmitDrawCommand(EntityType type, Ref<DrawCommand> drawCommand) override;
		Ref<Image> GetFinalImage() override;

	private:
		void Flush();
		void PrepareBuffers();
		void PrepareImages() const;
		void UpdateDescriptors();

		void GuiPass();
		void PreDepthPass();
		void EnvPass();
		void SkyboxPass();
		void GeometryPass();
		void DebugLinePass();
		void CompositePass();

	private:
		RenderSpecification m_RenderSpecification;
		Ref<Scene> m_Scene;

		Ref<CommandBuffer> m_CommandBuffer;
		Ref<DebugRenderer> m_DebugRenderer;

		Ref<Pipeline> m_PreDepthPipeline;
		Ref<Pipeline> m_EnvPipeline;
		Ref<Pipeline> m_SkyboxPipeline;
		Ref<Pipeline> m_GeometryPipeline;
		Ref<Pipeline> m_DebugLinePipeline;
		Ref<Pipeline> m_CompositePipeline;

		static constexpr uint32_t s_MaxLights = 8;

		// Frame in flight --> Set
		std::unordered_map<uint32_t, std::array<VkDescriptorSet, 4>> m_DescriptorSets;

		// Set 0, Binding 0
		struct EnvironmentData
		{
			glm::mat4 Projection;
			std::array<glm::mat4, 6> View;
		} m_EnvironmentBuffer;
		Ref<UniformBuffer> m_EnvironmentUB;

		// Set 0, Binding 1
		Ref<Image> m_EnvironmentMap;
		// Set 0, Binding 2
		Ref<Image> m_EnvironmentCubeMap;

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
		struct LightsData
		{
			glm::mat4 Projection;
			PointLight Lights[s_MaxLights];
			uint32_t NumLights;
		} m_LightsBuffer;
		Ref<UniformBuffer> m_LightsUB;

		// Set 1, Binding 2
		std::array<Ref<Image>, s_MaxLights> m_ShadowMaps;

		// Draw commands
		std::unordered_map<EntityType, std::vector<Ref<DrawCommand>>> m_DrawList;

		// Buffers
		Buffer m_PushConstantBuffer;
		Ref<VertexBuffer> m_DebugLineVertexBuffer;
		Ref<IndexBuffer> m_DebugLineIndexBuffer;
		uint32_t m_DebugLineCount = 0;

		// Statistics
		RenderStatistics m_RenderStatistics;

		struct TimestampQueries
		{
			uint32_t CompositeQuery = UINT32_MAX;
			uint32_t DebugLineQuery = UINT32_MAX;
			uint32_t GeometryQuery = UINT32_MAX;
			uint32_t PreDepthQuery = UINT32_MAX;
			uint32_t SkyboxQuery = UINT32_MAX;
		} m_TimestampQueries;
	};
}
