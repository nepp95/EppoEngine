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
		EPPO_PROFILE_FUNCTION("SceneRenderer::SceneRenderer");

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
		m_DirectionalLightUB = CreateRef<UniformBuffer>(sizeof(DirectionalLightData), 2);
		m_MaterialUB = CreateRef<UniformBuffer>(sizeof(MaterialData), 4);
	}

	void SceneRenderer::RenderGui()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::RenderGui");

		ImGui::Begin("Performance");

		ImGui::Text("GPU Time: %.3fms", (float)m_CommandBuffer->GetTimestamp() * 0.000001f);

		ImGui::Separator();

		ImGui::Text("Draw calls: %u", m_RenderStatistics.DrawCalls);
		ImGui::Text("Meshes: %u", m_RenderStatistics.Meshes);

		ImGui::End();
	}

	void SceneRenderer::Resize(uint32_t width, uint32_t height)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::Resize");

		m_GeometryFramebuffer->Resize(width, height);
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::BeginScene");

		m_CommandBuffer->RT_Begin();

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraBuffer.Position = glm::vec4(editorCamera.GetPosition(), 0.0f);

		// Cleanup from last draw
		m_DrawList.clear();
	}

	void SceneRenderer::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::BeginScene");

		m_CommandBuffer->RT_Begin();

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = glm::inverse(transform);
		m_CameraBuffer.Projection = camera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
		m_CameraBuffer.Position = transform[3];

		// Cleanup from last draw
		m_DrawList.clear();
	}

	void SceneRenderer::EndScene()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::EndScene");

		Flush();
	}

	void SceneRenderer::SubmitDirectionalLight(const DirectionalLightComponent& dlc)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::SubmitDirectionalLight");

		m_DirectionalLightBuffer.View = glm::lookAt(glm::vec3(0.0f, 10.0f, 20.0f), dlc.Direction, glm::vec3(0.0f, 1.0f, 0.0f));
		m_DirectionalLightBuffer.Projection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, 0.1f, 100.0f);
		m_DirectionalLightBuffer.Direction = glm::vec4(dlc.Direction, 0.0f);
		m_DirectionalLightBuffer.AmbientColor = dlc.AmbientColor;
		m_DirectionalLightBuffer.DiffuseColor = dlc.AlbedoColor;
		m_DirectionalLightBuffer.SpecularColor = dlc.SpecularColor;
	}

	void SceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::SubmitMesh");

		auto& drawCommand = m_DrawList.emplace_back();
		drawCommand.Handle = entityId;
		drawCommand.Mesh = mesh;
		drawCommand.Transform = transform;
	}

	void SceneRenderer::Flush()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::Flush");

		PrepareRender();
		
		PreDepthPass();
		GeometryPass();

		m_CommandBuffer->RT_End();
		m_CommandBuffer->RT_Submit();
	}

	void SceneRenderer::PrepareRender()
	{
		Renderer::SubmitCommand([this]()
		{
			EPPO_PROFILE_FUNCTION("SceneRenderer::PrepareRender");

			m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));
			m_DirectionalLightUB->SetData(&m_DirectionalLightBuffer, sizeof(DirectionalLightData));
			
			// Clear framebuffers
			m_GeometryFramebuffer->Bind();
			Renderer::Clear();
			m_GeometryFramebuffer->Unbind();

			m_PreDepthFramebuffer->Bind();
			Renderer::Clear(false, true);
			m_PreDepthFramebuffer->Unbind();
		});
	}

	void SceneRenderer::PreDepthPass()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::PreDepthPass");

		Renderer::SubmitCommand([this]()
		{
			Renderer::GetShader("predepth")->Bind();

			m_PreDepthFramebuffer->Bind();
			Renderer::SetFaceCulling(FaceCulling::FRONT);
			
			for (auto& dc : m_DrawList)
			{
				m_TransformUB->SetData(&dc.Transform, sizeof(glm::mat4));
				m_ShadowMap->Bind();

				for (auto& submesh : dc.Mesh->GetSubmeshes())
				{
					Ref<Material> material = dc.Mesh->GetMaterial(submesh->GetMaterialIndex());
					m_MaterialBuffer.AmbientColor = glm::vec4(material->m_AmbientColor, 0.0f);
					m_MaterialBuffer.DiffuseColor = glm::vec4(material->m_DiffuseColor, 0.0f);
					m_MaterialBuffer.SpecularColor = glm::vec4(material->m_SpecularColor, 0.0f);

					m_MaterialUB->SetData(&m_MaterialBuffer);

					Renderer::RenderGeometry(m_CommandBuffer, submesh);
					m_RenderStatistics.DrawCalls++;
				}
			}

			Renderer::SetFaceCulling(FaceCulling::BACK);
			m_PreDepthFramebuffer->Unbind();
		});
	}

	void SceneRenderer::GeometryPass()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::GeometryPass");

		Renderer::SubmitCommand([this]()
		{
			Renderer::GetShader("geometry")->Bind();

			m_GeometryFramebuffer->Bind();

			for (auto& dc : m_DrawList)
			{
				m_TransformUB->SetData(&dc.Transform, sizeof(glm::mat4));
				m_ShadowMap->Bind();

				for (auto& submesh : dc.Mesh->GetSubmeshes())
				{
					Ref<Material> material = dc.Mesh->GetMaterial(submesh->GetMaterialIndex());
					m_MaterialBuffer.AmbientColor = glm::vec4(material->m_AmbientColor, 0.0f);
					m_MaterialBuffer.DiffuseColor = glm::vec4(material->m_DiffuseColor, 0.0f);
					m_MaterialBuffer.SpecularColor = glm::vec4(material->m_SpecularColor, 0.0f);

					m_MaterialUB->SetData(&m_MaterialBuffer);

					Renderer::RenderGeometry(m_CommandBuffer, submesh);
					m_RenderStatistics.DrawCalls++;
					m_RenderStatistics.Meshes++;
				}
			}

			m_GeometryFramebuffer->Unbind();
		});
	}
}
