#include "pch.h"
#include "SceneRenderer.h"

#include "Renderer/Renderer.h"
#include "Scene/Scene.h"

#include <imgui.h>

namespace Eppo
{
	SceneRenderer::SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification)
		: m_RenderSpecification(renderSpecification)
	{
		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		m_CommandBuffer = CreateRef<RenderCommandBuffer>();

		// Framebuffer & Pipeline
		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = { ImageFormat::RGBA8, ImageFormat::Depth };
		framebufferSpec.Width = swapchain->GetWidth();
		framebufferSpec.Height = swapchain->GetHeight();
		framebufferSpec.Clear = true;
		framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		PipelineSpecification geometryPipelineSpec;
		geometryPipelineSpec.Framebuffer = CreateRef<Framebuffer>(framebufferSpec);
		geometryPipelineSpec.Shader = Renderer::GetShader("geometry");
		geometryPipelineSpec.Layout = {
			{ ShaderDataType::Float3, "inPosition" },
			{ ShaderDataType::Float3, "inNormal" },
			{ ShaderDataType::Float2, "inTexCoord" },
		};
		geometryPipelineSpec.DepthTesting = true;
		geometryPipelineSpec.PushConstants = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) },
			{ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float) }
		};

		m_GeometryPipeline = CreateRef<Pipeline>(geometryPipelineSpec);

		// Camera
		m_CameraUniformBuffer = CreateRef<UniformBuffer>(geometryPipelineSpec.Shader, sizeof(CameraData));

		// Vertex buffer
		//m_TransformBuffers.resize(VulkanConfig::MaxFramesInFlight);
		//m_VertexBuffer = CreateRef<VertexBuffer>(sizeof(glm::mat4) * 1000);
	}

	void SceneRenderer::RenderGui()
	{
		ImGui::Begin("Performance");

		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

		ImGui::Text("GPU Time: %.3fms", m_CommandBuffer->GetTimestamp(imageIndex));
		ImGui::Text("Geometry Pass: %.3fms", m_CommandBuffer->GetTimestamp(imageIndex, m_TimestampQueries.GeometryQuery));

		ImGui::Separator();

		const auto& pipelineStats = m_CommandBuffer->GetPipelineStatistics(imageIndex);
		ImGui::Text("Pipeline statistics:");
		ImGui::Text("Input Assembly Vertices: %llu", pipelineStats.InputAssemblyVertices);
		ImGui::Text("Input Assembly Primitives: %llu", pipelineStats.InputAssemblyPrimitives);
		ImGui::Text("Vertex Shader Invocations: %llu", pipelineStats.VertexShaderInvocations);
		ImGui::Text("Clipping Invocations: %llu", pipelineStats.ClippingInvocations);
		ImGui::Text("Clipping Primitives: %llu", pipelineStats.ClippingPrimitives);
		ImGui::Text("Fragment Shader Invocations: %llu", pipelineStats.FragmentShaderInvocations);

		ImGui::End();
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		// Prepare scene rendering
		m_CameraBuffer.ViewMatrix = editorCamera.GetViewMatrix();
		m_CameraBuffer.ViewProjectionMatrix = editorCamera.GetViewProjectionMatrix();
		m_CameraUniformBuffer->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Cleanup from last draw
		m_DrawList.clear(); // TODO: Do this at flush or begin scene?
	}

	void SceneRenderer::EndScene()
	{
		Flush();
	}

	Ref<Image> SceneRenderer::GetFinalPassImage()
	{
		return m_GeometryPipeline->GetSpecification().Framebuffer->GetFinalImage();
	}

	void SceneRenderer::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, EntityHandle entityId)
	{
		auto& drawCommand = m_DrawList[entityId];
		drawCommand.Mesh = mesh;
		drawCommand.Transform = transform;
	}

	void SceneRenderer::Flush()
	{
		m_CommandBuffer->Begin();
		m_TimestampQueries.GeometryQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryPipeline);

		for (auto& [entity, dc] : m_DrawList)
			Renderer::RenderGeometry(m_CommandBuffer, m_GeometryPipeline, m_CameraUniformBuffer, dc.Mesh, dc.Transform);

		Renderer::EndRenderPass(m_CommandBuffer);

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.GeometryQuery);
		m_CommandBuffer->End();
		m_CommandBuffer->Submit();
	}
}
