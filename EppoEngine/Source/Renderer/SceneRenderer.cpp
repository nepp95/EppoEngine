#include "pch.h"
#include "SceneRenderer.h"

#include "Renderer/Renderer.h"
#include "Scene/Scene.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace Eppo
{
	SceneRenderer::SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification)
		: m_RenderSpecification(renderSpecification)
	{
		m_CommandBuffer = CreateRef<RenderCommandBuffer>();

		// Geometry
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
			framebufferSpec.Width = 1920; // todo: make configurable
			framebufferSpec.Height = 1080; // todo: make configurable

			m_Framebuffer = CreateRef<Framebuffer>(framebufferSpec);

			/*pipelineSpec.Shader = Renderer::GetShader("geometry");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" }
			};*/
		}

		// PreDepth
		{
			// TODO: Create shadow map image
			// TODO: Clear pass for clearing the shadow map image
			/*FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { ImageFormat::Depth };
			framebufferSpec.Width = swapchain->GetWidth();
			framebufferSpec.Height = swapchain->GetHeight();

			pipelineSpec.Shader = Renderer::GetShader("shadow");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" }
			};*/
		}

		// Uniform buffers
		m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), 0);
		m_TransformUB = CreateRef<UniformBuffer>(sizeof(glm::mat4), 1);
		m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), 2);
	}

	void SceneRenderer::RenderGui()
	{
		ImGui::Begin("Performance");

		/*ImGui::Text("GPU Time: %.3fms", m_CommandBuffer->GetTimestamp(imageIndex));
		ImGui::Text("Geometry Pass: %.3fms", m_CommandBuffer->GetTimestamp(imageIndex, m_TimestampQueries.GeometryQuery));
		ImGui::Text("Shadow Pass: %.3fms", m_CommandBuffer->GetTimestamp(imageIndex, m_TimestampQueries.ShadowQuery));

		ImGui::Separator();

		const auto& pipelineStats = m_CommandBuffer->GetPipelineStatistics(imageIndex);
		ImGui::Text("Pipeline statistics:");
		ImGui::Text("Input Assembly Vertices: %llu", pipelineStats.InputAssemblyVertices);
		ImGui::Text("Input Assembly Primitives: %llu", pipelineStats.InputAssemblyPrimitives);
		ImGui::Text("Vertex Shader Invocations: %llu", pipelineStats.VertexShaderInvocations);
		ImGui::Text("Clipping Invocations: %llu", pipelineStats.ClippingInvocations);
		ImGui::Text("Clipping Primitives: %llu", pipelineStats.ClippingPrimitives);
		ImGui::Text("Fragment Shader Invocations: %llu", pipelineStats.FragmentShaderInvocations);*/

		ImGui::End();
	}

	void SceneRenderer::Resize(uint32_t width, uint32_t height)
	{
		m_Framebuffer->Resize(width, height);
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Environment UB
		m_EnvironmentBuffer.LightView = glm::lookAt(m_EnvironmentBuffer.LightPosition, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.LightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 125.0f);
		m_EnvironmentBuffer.LightViewProjection = m_EnvironmentBuffer.LightProjection * m_EnvironmentBuffer.LightView;
		m_EnvironmentUB->SetData(&m_EnvironmentBuffer, sizeof(m_EnvironmentBuffer));

		// Cleanup from last draw
		m_DrawList.clear(); // TODO: Do this at flush or begin scene?
	}

	void SceneRenderer::EndScene()
	{
		Flush();
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

		PrepareRender();
		
		ShadowPass();
		GeometryPass();

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();
	}

	void SceneRenderer::PrepareRender()
	{
		m_Framebuffer->Bind();

		Renderer::Clear();

		m_Framebuffer->Unbind();
	}

	void SceneRenderer::GeometryPass()
	{
		EntityHandle handle;
		for (auto& [entity, dc] : m_DrawList)
		{
			m_Framebuffer->Bind();

			m_TransformUB->SetData(&dc.Transform, sizeof(glm::mat4));

			Renderer::RenderGeometry(m_CommandBuffer, dc.Mesh);
			handle = entity;

			m_Framebuffer->Unbind();

			m_RenderStatistics.DrawCalls++;
			m_RenderStatistics.Meshes++;
		}

		//glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_EnvironmentBuffer.LightPosition));
		//Renderer::RenderGeometry(m_CommandBuffer, m_GeometryPipeline, m_EnvironmentUniformBuffer, m_CameraUniformBuffer, m_DrawList[handle].Mesh, transform);*/
	}

	void SceneRenderer::ShadowPass()
	{
		//Renderer::SubmitCommand([this]()
		//{
		//	for (auto& [entity, dc] : m_DrawList)
		//	{
		//		for (const auto& submesh : dc.Mesh->GetSubmeshes())
		//		{
		//			// Vertex buffer Mesh
		//			VkBuffer vb = { submesh.GetVertexBuffer()->GetBuffer() };
		//			VkDeviceSize offsets[] = { 0 };
		//			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

		//			// Index buffer
		//			Ref<IndexBuffer> indexBuffer = submesh.GetIndexBuffer();
		//			vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

		//			// Push constants
		//			vkCmdPushConstants(commandBuffer, m_ShadowPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &dc.Transform);
		//		
		//			// Draw call
		//			vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
		//		}
		//	}
		//});
	}
}
