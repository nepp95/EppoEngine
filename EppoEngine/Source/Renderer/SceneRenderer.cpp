#include "pch.h"
#include "SceneRenderer.h"

// TODO: TEMP
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

#include "Renderer/RenderCommandBuffer.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace Eppo
{
	SceneRenderer::SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification)
		: m_RenderSpecification(renderSpecification)
	{
		m_CommandBuffer = RenderCommandBuffer::Create();

		// Geometry
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { ImageFormat::RGBA8, ImageFormat::Depth };
			framebufferSpec.ClearColorOnLoad = true;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			framebufferSpec.ClearDepthOnLoad = true;
			framebufferSpec.ClearDepth = 1.0f;

			PipelineSpecification pipelineSpec;
			pipelineSpec.Framebuffer = Framebuffer::Create(framebufferSpec);
			pipelineSpec.Shader = Renderer::GetShader("geometry");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" }
			};
			pipelineSpec.DepthTesting = true;
			pipelineSpec.PushConstants = {
				{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) },
				{ VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3) + sizeof(float) }
			};

			m_GeometryPipeline = Pipeline::Create(pipelineSpec);
		}

		// Lighting
		{
			FramebufferSpecification framebufferSpec;
		}

		// PreDepth
		{
			// TODO: Create shadow map image
			// TODO: Clear pass for clearing the shadow map image

			FramebufferSpecification framebufferSpec;
			framebufferSpec.Attachments = { ImageFormat::Depth };
			framebufferSpec.ClearColorOnLoad = false;
			framebufferSpec.ClearDepthOnLoad = true;
			framebufferSpec.ClearDepth = 1.0f;

			PipelineSpecification pipelineSpec;
			pipelineSpec.Framebuffer = Framebuffer::Create(framebufferSpec);
			pipelineSpec.Shader = Renderer::GetShader("shadow");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" }
			};
			pipelineSpec.DepthTesting = true;
			pipelineSpec.PushConstants = {
				{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) }
			};

			m_ShadowPipeline = Pipeline::Create(pipelineSpec);
		}

		// Uniform buffers
		m_UniformBufferSet = Ref<UniformBufferSet>::Create();
		// TODO: Set doesn't solve the problem of having one uniform buffer for multiple different pipeline layouts
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			m_UniformBufferSet->Set(UniformBuffer::Create(sizeof(CameraData), 0), i, 0);
			m_UniformBufferSet->Set(UniformBuffer::Create(sizeof(EnvironmentData), 1), i, 0);
		}
		
		m_CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
		m_EnvironmentUniformBuffer = UniformBuffer::Create(sizeof(EnvironmentData), 1);
	}

	void SceneRenderer::RenderGui()
	{
		ImGui::Begin("Performance");

		uint32_t imageIndex = Renderer::GetCurrentFrameIndex();

		ImGui::Text("GPU Time: %.3fms", m_CommandBuffer->GetTimestamp(imageIndex));
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
		ImGui::Text("Fragment Shader Invocations: %llu", pipelineStats.FragmentShaderInvocations);

		ImGui::End();
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		// Prepare scene rendering
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraUniformBuffer->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Matrices
		m_EnvironmentBuffer.LightView = glm::lookAt(m_EnvironmentBuffer.LightPosition, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.LightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 125.0f);
		m_EnvironmentBuffer.LightViewProjection = m_EnvironmentBuffer.LightProjection * m_EnvironmentBuffer.LightView;

		m_EnvironmentUniformBuffer->SetData(&m_EnvironmentBuffer, sizeof(m_EnvironmentBuffer));

		// Cleanup from last draw
		m_DrawList.clear(); // TODO: Do this at flush or begin scene?
	}

	void SceneRenderer::EndScene()
	{
		Flush();
	}

	void SceneRenderer::SetEnvironment(EnvironmentKeys key, void* value)
	{
		switch (key)
		{
			case EnvironmentKeys::LightPosition:	m_EnvironmentBuffer.LightPosition = *(glm::vec3*)value; break;
			case EnvironmentKeys::LightColor:		m_EnvironmentBuffer.LightColor = *(glm::vec4*)value; break;
		}
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

		//PrepareRender();
		
		ShadowPass();
		GeometryPass();

		m_CommandBuffer->End();
		m_CommandBuffer->Submit();
	}

	void SceneRenderer::PrepareRender()
	{

	}

	void SceneRenderer::GeometryPass()
	{
		m_TimestampQueries.GeometryQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::BeginRenderPass(m_CommandBuffer, m_GeometryPipeline);

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		// Descriptor sets
		// TODO: Update materials through method
		Ref<VulkanPipeline> vulkanPipeline = m_GeometryPipeline.As<VulkanPipeline>();
		vulkanPipeline->UpdateDescriptors(frameIndex, m_UniformBufferSet);

		EntityHandle handle;
		for (auto& [entity, dc] : m_DrawList)
		{
			Renderer::RenderGeometry(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, dc.Mesh, dc.Transform);
			handle = entity;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(m_EnvironmentBuffer.LightPosition));
		Renderer::RenderGeometry(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_DrawList[handle].Mesh, transform);

		Renderer::EndRenderPass(m_CommandBuffer);

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.GeometryQuery);
	}

	void SceneRenderer::ShadowPass()
	{
		m_TimestampQueries.ShadowQuery = m_CommandBuffer->BeginTimestampQuery();

		// TODO: Clear before?
		Renderer::BeginRenderPass(m_CommandBuffer, m_ShadowPipeline);

		Renderer::SubmitCommand([this]()
		{
			Ref<VulkanContext> context = RendererContext::Get().As<VulkanContext>();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = m_CommandBuffer.As<VulkanRenderCommandBuffer>()->GetCurrentCommandBuffer();
			uint32_t frameIndex = swapchain->GetCurrentImageIndex();

			// Descriptor sets
			const auto& descriptorSets = m_ShadowPipeline.As<VulkanPipeline>()->GetDescriptorSets(frameIndex);

			std::vector<VkWriteDescriptorSet> writeDescriptors;
			{
				VkWriteDescriptorSet& writeDescriptor = writeDescriptors.emplace_back();
				writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptor.descriptorCount = 1;
				writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptor.dstSet = descriptorSets[0];
				writeDescriptor.dstBinding = 1;
				writeDescriptor.pBufferInfo = &m_EnvironmentUniformBuffer.As<VulkanUniformBuffer>()->GetDescriptorBufferInfo();
			}

			VkDevice device = context->GetLogicalDevice()->GetNativeDevice();
			vkUpdateDescriptorSets(device, writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

			// Pipeline
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline.As<VulkanPipeline>()->GetPipeline());

			VkExtent2D extent = swapchain->GetExtent();

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)extent.width;
			viewport.height = (float)extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = extent;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_ShadowPipeline.As<VulkanPipeline>()->GetPipelineLayout(),
				0,
				1,
				&descriptorSets[0],
				0,
				nullptr
			);

			for (auto& [entity, dc] : m_DrawList)
			{
				for (const auto& submesh : dc.Mesh->GetSubmeshes())
				{
					// Vertex buffer Mesh
					VkBuffer vb = { submesh.GetVertexBuffer().As<VulkanVertexBuffer>()->GetBuffer() };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

					// Index buffer
					Ref<IndexBuffer> indexBuffer = submesh.GetIndexBuffer();
					vkCmdBindIndexBuffer(commandBuffer, indexBuffer.As<VulkanIndexBuffer>()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

					// Push constants
					vkCmdPushConstants(commandBuffer, m_ShadowPipeline.As<VulkanPipeline>()->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &dc.Transform);
				
					// Draw call
					vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
				}
			}
		});

		Renderer::EndRenderPass(m_CommandBuffer);

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.ShadowQuery);
	}
}
