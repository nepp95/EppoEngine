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
		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();
		VkDevice device = context->GetLogicalDevice()->GetNativeDevice();

		m_CommandBuffer = CreateRef<RenderCommandBuffer>();

		// Geometry
		{
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
		}

		// Lighting
		{
			FramebufferSpecification framebufferSpec;
		}

		// Shadow
		{
			// TODO: Create shadow map image
			// TODO: Clear pass for clearing the shadow map image

			FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { ImageFormat::Depth };
			framebufferSpec.Width = swapchain->GetWidth();
			framebufferSpec.Height = swapchain->GetHeight();
			framebufferSpec.Clear = true;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

			PipelineSpecification shadowPipelineSpec;
			shadowPipelineSpec.Framebuffer = CreateRef<Framebuffer>(framebufferSpec);
			shadowPipelineSpec.Shader = Renderer::GetShader("lighting");
			shadowPipelineSpec.Layout = {

			};
		}

		// Uniform buffers
		m_CameraUniformBuffer = CreateRef<UniformBuffer>(sizeof(CameraData));
		m_EnvironmentUniformBuffer = CreateRef<UniformBuffer>(sizeof(EnvironmentData));

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

		// Set environment
		m_EnvironmentBuffer.LightPosition.z = 0.0f;
		if (m_EnvironmentBuffer.LightPosition.x > 50.0f)
			m_EnvironmentBuffer.LightPosition.x = -50.0f;
		else
			m_EnvironmentBuffer.LightPosition.x += 0.1f;

		if (m_EnvironmentBuffer.LightPosition.y > 50.0f)
			m_EnvironmentBuffer.LightPosition.y = -50.0f;
		else
			m_EnvironmentBuffer.LightPosition.y += 0.1f;

		m_EnvironmentUniformBuffer->SetData(&m_EnvironmentBuffer, sizeof(m_EnvironmentBuffer));

		EPPO_WARN("{}", m_EnvironmentBuffer.LightPosition);

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

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_GeometryPipeline->GetSpecification().Shader->GetDescriptorSetLayout(0);
		allocInfo.pNext = nullptr;

		VkDescriptorSet set0 = Renderer::AllocateDescriptorSet(allocInfo);

		allocInfo.pSetLayouts = &m_GeometryPipeline->GetSpecification().Shader->GetDescriptorSetLayout(1);
		VkDescriptorSet set1 = Renderer::AllocateDescriptorSet(allocInfo);

		{
			VkWriteDescriptorSet writeDescriptor{};
			writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptor.dstSet = set0;
			writeDescriptor.dstBinding = 0;
			writeDescriptor.pBufferInfo = &m_EnvironmentUniformBuffer->GetDescriptorBufferInfo();

			VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
			vkUpdateDescriptorSets(device, 1, &writeDescriptor, 0, nullptr);
		}

		{
			VkWriteDescriptorSet writeDescriptor{};
			writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptor.dstSet = set1;
			writeDescriptor.dstBinding = 0;
			writeDescriptor.pBufferInfo = &m_CameraUniformBuffer->GetDescriptorBufferInfo();

			VkDevice device = RendererContext::Get()->GetLogicalDevice()->GetNativeDevice();
			vkUpdateDescriptorSets(device, 1, &writeDescriptor, 0, nullptr);
		}

		for (auto& [entity, dc] : m_DrawList)
			Renderer::RenderGeometry(m_CommandBuffer, m_GeometryPipeline, set0, set1, dc.Mesh, dc.Transform);

		Renderer::EndRenderPass(m_CommandBuffer);

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.GeometryQuery);
		m_CommandBuffer->End();
		m_CommandBuffer->Submit();
	}
}
