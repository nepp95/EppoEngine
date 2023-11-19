#include "pch.h"
#include "SceneRenderer.h"

#include "Renderer/Renderer.h"
#include "Scene/Scene.h"

namespace Eppo
{
	SceneRenderer::SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification)
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
		framebufferSpec.ClearColor = { 0.4f, 0.4f, 0.4f, 1.0f };

		// TODO: Load from shader library in renderer
		ShaderSpecification geometryShaderSpec;
		geometryShaderSpec.ShaderSources = {
			{ ShaderType::Vertex, "Resources/Shaders/geometry.vert" },
			{ ShaderType::Fragment, "Resources/Shaders/geometry.frag" },
		};

		PipelineSpecification geometryPipelineSpec;
		geometryPipelineSpec.Framebuffer = CreateRef<Framebuffer>(framebufferSpec);
		geometryPipelineSpec.Shader = CreateRef<Shader>(geometryShaderSpec, s_Data->DescriptorCache); // Solution: Create shader descriptor sets. One pool per shader. Investigation needed
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
		//m_VertexBuffer = CreateRef<VertexBuffer>(sizeof(glm::mat4) * 1000);
	}

	void SceneRenderer::BeginScene()
	{
		// Prepare scene rendering
	}

	void SceneRenderer::EndScene()
	{
		Flush();
	}

	void SceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId)
	{
		auto& drawCommand = m_DrawList[entityId];
		drawCommand.Mesh = mesh;
		drawCommand.Transform = transform;
	}

	void SceneRenderer::Flush()
	{
		PrepareRender();

		m_CommandBuffer->Begin();

		m_TimestampQueries.GeometryQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryPipeline);

		for (auto& [entity, dc] : m_DrawList)
			Renderer::RenderGeometry(m_CommandBuffer, m_GeometryPipeline, dc.Mesh);

		Renderer::EndRenderPass();

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.GeometryQuery);

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();

		// Clear drawlist and update stats
		m_DrawList.clear();
	}

	void SceneRenderer::PrepareRender()
	{
		// Fill transform buffer
		glm::mat4* buffer = new glm::mat4[m_DrawList.size()];

		for (auto& [entity, dc] : m_DrawList)
		{
			*buffer = dc.Transform;
			buffer++;
		}
		
		Renderer::SubmitCommand([this, buffer]()
		{
			Ref<Swapchain> swapchain = RendererContext::Get()->GetSwapchain();
			uint32_t imageIndex = swapchain->GetCurrentImageIndex();

			m_TransformBuffers[imageIndex]->SetData((void*)buffer, m_DrawList.size() * sizeof(glm::mat4));
		});
	}
}
