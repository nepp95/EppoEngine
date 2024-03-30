#include "pch.h"
#include "SceneRenderer.h"

#include "Asset/AssetManager.h"
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
			TextureSpecification textureSpec;
			textureSpec.Format = TextureFormat::Depth;
			textureSpec.Wrap = TextureWrap::CLAMP_TO_BORDER;
			textureSpec.BorderColor = glm::vec4(1.0f);
			textureSpec.Width = 2048;
			textureSpec.Height = 2048;

			m_ShadowMap = CreateRef<Texture>(textureSpec);

			FramebufferSpecification framebufferSpec;
			framebufferSpec.ExistingDepthTexture = m_ShadowMap;
			framebufferSpec.Width = 2048;
			framebufferSpec.Height = 2048;

			m_PreDepthFramebuffer = CreateRef<Framebuffer>(framebufferSpec);
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
		ImGui::Text("Light position: %.3f, %.3f, %.3f", m_EnvironmentBuffer.LightPosition.x, m_EnvironmentBuffer.LightPosition.y, m_EnvironmentBuffer.LightPosition.z);

		ImGui::End();
	}

	void SceneRenderer::Resize(uint32_t width, uint32_t height)
	{
		m_GeometryFramebuffer->Resize(width, height);
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		m_CommandBuffer->RT_Begin();

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraBuffer.Position = editorCamera.GetPosition();
		m_CameraUB->RT_SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Environment UB
		m_EnvironmentBuffer.LightView = glm::lookAt(m_EnvironmentBuffer.LightPosition, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.LightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, 0.1f, 100.0f);
		m_EnvironmentBuffer.LightViewProjection = m_EnvironmentBuffer.LightProjection * m_EnvironmentBuffer.LightView;
		m_EnvironmentUB->RT_SetData(&m_EnvironmentBuffer, sizeof(m_EnvironmentBuffer));

		// Cleanup from last draw
		m_DrawList.clear(); // TODO: Do this at flush or begin scene?
	}

	void SceneRenderer::EndScene()
	{
		Flush();
	}

	void SceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId)
	{
		auto& drawCommand = m_DrawList.emplace_back();
		drawCommand.Handle = entityId;
		drawCommand.Mesh = mesh;
		drawCommand.Transform = transform;
	}

	void SceneRenderer::Flush()
	{
		PrepareRender();
		
		PreDepthPass();
		GeometryPass();

		m_CommandBuffer->RT_End();
		m_CommandBuffer->RT_Submit();
	}

	void SceneRenderer::PrepareRender()
	{
		// Render light position
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_EnvironmentBuffer.LightPosition);
		auto& mesh = m_DrawList[0];
		//SubmitMesh(transform, mesh.Mesh, EntityHandle());

		if (temp)
		{
			if (m_EnvironmentBuffer.LightPosition.x > 25.0f)
				temp = false;
			m_EnvironmentBuffer.LightPosition.x += 0.01f;
		}
		else
		{
			if (m_EnvironmentBuffer.LightPosition.x < -25.0f)
				temp = true;
			m_EnvironmentBuffer.LightPosition.x -= 0.01f;
		}

		// Clear framebuffers
		m_GeometryFramebuffer->RT_Bind();
		Renderer::RT_Clear();
		m_GeometryFramebuffer->RT_Unbind();

		m_PreDepthFramebuffer->RT_Bind();
		Renderer::RT_Clear(false, true);
		m_PreDepthFramebuffer->RT_Unbind();
	}

	void SceneRenderer::PreDepthPass()
	{
		Renderer::GetShader("predepth")->RT_Bind();

		m_PreDepthFramebuffer->RT_Bind();
		Renderer::RT_SetFaceCulling(FaceCulling::FRONT);

		for (auto& dc : m_DrawList)
		{
			m_TransformUB->RT_SetData(&dc.Transform, sizeof(glm::mat4));

			Renderer::RT_RenderGeometry(m_CommandBuffer, m_MaterialUB, dc.Mesh);

			m_RenderStatistics.DrawCalls++;
		}

		Renderer::RT_SetFaceCulling(FaceCulling::BACK);
		m_PreDepthFramebuffer->RT_Unbind();
	}

	void SceneRenderer::GeometryPass()
	{
		Renderer::GetShader("geometry")->RT_Bind();

		m_GeometryFramebuffer->RT_Bind();

		for (auto& dc : m_DrawList)
		{
			m_TransformUB->RT_SetData(&dc.Transform, sizeof(glm::mat4));
			m_ShadowMap->RT_Bind();

			Renderer::RT_RenderGeometry(m_CommandBuffer, m_MaterialUB, dc.Mesh);

			m_RenderStatistics.DrawCalls++;
			m_RenderStatistics.Meshes++;
		}

		m_GeometryFramebuffer->RT_Unbind();
	}
}
