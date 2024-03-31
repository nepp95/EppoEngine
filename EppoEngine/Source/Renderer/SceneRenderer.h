#pragma once

#include "Renderer/Camera/EditorCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/UniformBuffer.h"

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

		void Resize(uint32_t width, uint32_t height);

		void BeginScene(const EditorCamera& editorCamera);
		void EndScene();

		void SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId);

		uint32_t GetFinalImageID() const { return m_GeometryFramebuffer->GetColorAttachmentID(); }

	private:
		void Flush();

		void PrepareRender();

		void PreDepthPass();
		void GeometryPass();

	private:
		RenderSpecification m_RenderSpecification;

		// Command buffer
		Ref<RenderCommandBuffer> m_CommandBuffer;

		// Framebuffers
		Ref<Framebuffer> m_GeometryFramebuffer;
		Ref<Framebuffer> m_PreDepthFramebuffer;

		// Binding 0
		struct CameraData
		{
			glm::mat4 View;
			glm::mat4 Projection;
			glm::mat4 ViewProjection;
			glm::vec3 Position;
		} m_CameraBuffer;
		Ref<UniformBuffer> m_CameraUB;

		// Binding 1
		glm::mat4 m_Transform;
		Ref<UniformBuffer> m_TransformUB;

		// Binding 2
		struct EnvironmentData
		{
			glm::mat4 LightView;
			glm::mat4 LightProjection;
			glm::mat4 LightViewProjection;
			glm::vec3 LightPosition = glm::vec3(0.0f, 10.0f, -50.0f);
			glm::vec3 LightColor = glm::vec3(1.0f);
		} m_EnvironmentBuffer;
		Ref<UniformBuffer> m_EnvironmentUB;

		// Binding 3
		Ref<Texture> m_ShadowMap;

		// Binding 4
		struct MaterialData
		{
			glm::vec3 AlbedoColor;
			float Roughness;
		} m_MaterialBuffer;
		Ref<UniformBuffer> m_MaterialUB;

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



		bool temp = true;
	};
}
