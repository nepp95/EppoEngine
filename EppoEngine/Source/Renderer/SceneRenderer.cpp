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

			m_GeometryFramebuffer = CreateRef<Framebuffer>(framebufferSpec);
		}

		// PreDepth
		{
			m_ShadowMap = CreateRef<Texture>(1024, 1024);

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ExistingTextures = { m_ShadowMap };
			framebufferSpec.Width = 1024; // todo: make configurable
			framebufferSpec.Height = 1024; // todo: make configurable

			//m_PreDepthFramebuffer = CreateRef<Framebuffer>(framebufferSpec);
		}

		// Uniform buffers
		m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), 0);
		m_TransformUB = CreateRef<UniformBuffer>(sizeof(glm::mat4), 1);
		m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), 2);
		m_MaterialUB = CreateRef<UniformBuffer>(sizeof(MaterialData), 4);
	}

	void SceneRenderer::RenderGui()
	{
		ImGui::Begin("Performance");

		ImGui::Text("GPU Time: %.3fms", (float)m_CommandBuffer->GetTimestamp() * 0.000001f);

		ImGui::Separator();

		ImGui::Text("Draw calls: %u", m_RenderStatistics.DrawCalls);
		ImGui::Text("Meshes: %u", m_RenderStatistics.Meshes);

		ImGui::End();
	}

	void SceneRenderer::Resize(uint32_t width, uint32_t height)
	{
		m_GeometryFramebuffer->Resize(width, height);
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraUB->RT_SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Environment UB
		m_EnvironmentBuffer.LightColor = glm::vec3(1.0f);
		m_EnvironmentBuffer.LightPosition = glm::vec3(0.0f, 0.0f, -30.0f);
		m_EnvironmentBuffer.LightView = glm::lookAt(m_EnvironmentBuffer.LightPosition, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.LightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 125.0f);
		m_EnvironmentBuffer.LightViewProjection = m_EnvironmentBuffer.LightProjection * m_EnvironmentBuffer.LightView;
		m_EnvironmentUB->RT_SetData(&m_EnvironmentBuffer, sizeof(m_EnvironmentBuffer));

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
		m_CommandBuffer->RT_Begin();

		PrepareRender();
		
		//ShadowPass();
		GeometryPass();

		m_CommandBuffer->RT_End();
		m_CommandBuffer->RT_Submit();
	}

	void SceneRenderer::PrepareRender()
	{
		m_GeometryFramebuffer->RT_Bind();
		Renderer::RT_Clear();
		m_GeometryFramebuffer->RT_Unbind();

		//m_PreDepthFramebuffer->RT_Bind();
		//Renderer::RT_Clear();
		//m_PreDepthFramebuffer->RT_Unbind();
	}

	void SceneRenderer::GeometryPass()
	{
		Renderer::GetShader("geometry")->RT_Bind();

		m_GeometryFramebuffer->RT_Bind();

		EntityHandle handle;
		for (auto& [entity, dc] : m_DrawList)
		{
			m_TransformUB->RT_SetData(&dc.Transform, sizeof(glm::mat4));

			Renderer::RT_RenderGeometry(m_CommandBuffer, m_MaterialUB, dc.Mesh);
			handle = entity;

			m_RenderStatistics.DrawCalls++;
			m_RenderStatistics.Meshes++;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_EnvironmentBuffer.LightPosition));
		m_TransformUB->RT_SetData(&transform, sizeof(glm::mat4));

		Renderer::RT_RenderGeometry(m_CommandBuffer, m_MaterialUB, m_DrawList[handle].Mesh);

		m_GeometryFramebuffer->RT_Unbind();
	}

	void SceneRenderer::ShadowPass()
	{
		Renderer::GetShader("shadow")->RT_Bind();
		
		for (auto& [entity, dc] : m_DrawList)
		{
			m_PreDepthFramebuffer->RT_Bind();

			m_TransformUB->RT_SetData(&dc.Transform, sizeof(glm::mat4));
			// TODO: Bind rendererid from framebuffer whaaaaaaaat? GLBINDTEXTURE

			Renderer::RT_RenderGeometry(m_CommandBuffer, m_MaterialUB, dc.Mesh);

			m_PreDepthFramebuffer->RT_Unbind();
		}
	}
}
