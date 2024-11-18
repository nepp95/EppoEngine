#include "pch.h"
#include "SceneRenderer.h"

#include "Asset/AssetManager.h"
#include "Core/Application.h"
#include "ImGui/Image.h"
#include "Renderer/DescriptorWriter.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"
#include "Renderer/Vertex.h"
#include "Scene/Scene.h"

#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

namespace Eppo
{
	SceneRenderer::SceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpecification)
		: m_RenderSpecification(renderSpecification), m_Scene(scene)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::SceneRenderer");

		Ref<RendererContext> context = RendererContext::Get();
		Ref<Swapchain> swapchain = context->GetSwapchain();

		m_CommandBuffer = swapchain->GetCommandBuffer();

		// PreDepth
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.DepthTesting = true;
			pipelineSpec.DepthCubeMapImage = true;
			pipelineSpec.Width = 1024;
			pipelineSpec.Height = 1024;
			pipelineSpec.Shader = Renderer::GetShader("predepth");
			pipelineSpec.PushConstantRanges = pipelineSpec.Shader->GetPushConstantRanges();
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" },
				{ ShaderDataType::Float4, "inColor" }
			};

			m_PreDepthPipeline = CreateRef<Pipeline>(pipelineSpec);

			for (uint32_t i = 0; i < s_MaxLights; i++)
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::Depth;
				imageSpec.Width = 1024;
				imageSpec.Height = 1024;
				imageSpec.CubeMap = true;

				m_ShadowMaps[i] = CreateRef<Image>(imageSpec);

				VkCommandBuffer cmd = context->GetLogicalDevice()->GetCommandBuffer(true);
				Image::TransitionImage(cmd, m_ShadowMaps[i]->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
				context->GetLogicalDevice()->FlushCommandBuffer(cmd);
			}
		}

		// Geometry
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.ColorAttachments = { 
				{ ImageFormat::RGBA8, true, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) }
			};
			pipelineSpec.DepthTesting = true;
			pipelineSpec.Width = m_RenderSpecification.Width;
			pipelineSpec.Height = m_RenderSpecification.Height;
			pipelineSpec.Shader = Renderer::GetShader("geometry");
			pipelineSpec.PushConstantRanges = pipelineSpec.Shader->GetPushConstantRanges();
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" },
				{ ShaderDataType::Float4, "inColor" }
			};

			m_GeometryPipeline = CreateRef<Pipeline>(pipelineSpec);
		}

		// Composite
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.ColorAttachments = {
				{ ImageFormat::RGBA8, true, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) }
			};
			pipelineSpec.SwapchainTarget = true;
			pipelineSpec.Width = swapchain->GetWidth();
			pipelineSpec.Height = swapchain->GetHeight();
			pipelineSpec.Shader = Renderer::GetShader("composite");
			pipelineSpec.PushConstantRanges = pipelineSpec.Shader->GetPushConstantRanges();

			m_CompositePipeline = CreateRef<Pipeline>(pipelineSpec);
		}

		// Uniform buffers
		// Set 1
		m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), 0);
		m_LightsUB = CreateRef<UniformBuffer>(sizeof(LightsData), 1);
	}

	void SceneRenderer::RenderGui()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::RenderGui");

		ImGui::Begin("Performance");

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		ImGui::Text("GPU Time: %.3fms", m_CommandBuffer->GetTimestamp(frameIndex));
		ImGui::Text("PreDepth Pass: %.3fms", m_CommandBuffer->GetTimestamp(frameIndex, m_TimestampQueries.PreDepthQuery));
		ImGui::Text("Geometry Pass: %.3fms", m_CommandBuffer->GetTimestamp(frameIndex, m_TimestampQueries.GeometryQuery));
		ImGui::Text("Composite Pass: %.3fms", m_CommandBuffer->GetTimestamp(frameIndex, m_TimestampQueries.CompositeQuery));

		ImGui::Separator();

		const auto& pipelineStats = m_CommandBuffer->GetPipelineStatistics(frameIndex);

		ImGui::Text("Pipeline statistics:");
		ImGui::Text("Input Assembly Vertices: %llu", pipelineStats.InputAssemblyVertices);
		ImGui::Text("Input Assembly Primitives: %llu", pipelineStats.InputAssemblyPrimitives);
		ImGui::Text("Vertex Shader Invocations: %llu", pipelineStats.VertexShaderInvocations);
		ImGui::Text("Clipping Invocations: %llu", pipelineStats.ClippingInvocations);
		ImGui::Text("Clipping Primitives: %llu", pipelineStats.ClippingPrimitives);
		ImGui::Text("Fragment Shader Invocations: %llu", pipelineStats.FragmentShaderInvocations);

		ImGui::Separator();

		ImGui::Text("Draw calls: %u", m_RenderStatistics.DrawCalls);
		ImGui::Text("Meshes: %u", m_RenderStatistics.Meshes);
		ImGui::Text("Submeshes: %u", m_RenderStatistics.Submeshes);
		ImGui::Text("Instances: %u", m_RenderStatistics.MeshInstances);
		ImGui::Text("Camera position: %.2f, %.2f, %.2f", m_CameraBuffer.Position.x, m_CameraBuffer.Position.y, m_CameraBuffer.Position.z);

		ImGui::End();
	}

	void SceneRenderer::Resize(uint32_t width, uint32_t height)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::Resize");

		//m_GeometryFramebuffer->Resize(width, height);
	}

	void SceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::BeginScene");

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraBuffer.Position = glm::vec4(editorCamera.GetPosition(), 0.0f);
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Lights UB
		m_LightsBuffer.Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
		//m_LightsBuffer.Projection[1][1] *= -1.0f;
		m_LightsBuffer.NumLights = 2;

		for (uint32_t i = 0; i < m_LightsBuffer.NumLights; i++)
		{
			auto& light = m_LightsBuffer.Lights[i];
			glm::vec3 position;
			if (i == 0)
				position = { 5.0f, 1.0f, 0.0f };
			else
				position = { -10.0f, 1.0f, 0.0f };

			light.Position = glm::vec4(position, 1.0f);
			light.Color = glm::vec4(1.0f, 0.8f, 0.8f, 1.0f);

			light.View[0] = glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			light.View[1] = glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			light.View[2] = glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			light.View[3] = glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
			light.View[4] = glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			light.View[5] = glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		}

		m_LightsUB->SetData(&m_LightsBuffer, sizeof(LightsData));

		// Cleanup from last draw
		m_DrawList.clear();
	}

	void SceneRenderer::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::BeginScene");

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = glm::inverse(transform);
		m_CameraBuffer.Projection = camera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
		m_CameraBuffer.Position = transform[3];
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		// Lights UB
		m_LightsBuffer.Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
		//m_LightsBuffer.Projection[1][1] *= -1.0f;
		m_LightsBuffer.NumLights = 2;

		for (uint32_t i = 0; i < m_LightsBuffer.NumLights; i++)
		{
			auto& light = m_LightsBuffer.Lights[i];
			glm::vec3 position;
			if (i == 0)
				position = { 5.0f, 1.0f, 0.0f };
			else
				position = { -10.0f, 1.0f, 0.0f };

			light.Position = glm::vec4(position, 1.0f);
			light.Color = glm::vec4(0.8f, 0.4f, 0.4f, 1.0f);

			light.View[0] = glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			light.View[1] = glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			light.View[2] = glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			light.View[3] = glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
			light.View[4] = glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			light.View[5] = glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		}

		m_LightsUB->SetData(&m_LightsBuffer, sizeof(LightsData));

		// Cleanup from last draw
		m_DrawList.clear();
	}

	void SceneRenderer::EndScene()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::EndScene");

		Flush();
	}

	void SceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId)
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::SubmitMesh");

		auto& drawCommand = m_DrawList.emplace_back();
		drawCommand.Handle = entityId;
		drawCommand.Mesh = mesh;
		drawCommand.Transform = transform;
	}

	Ref<Image> SceneRenderer::GetFinalImage()
	{
		return m_GeometryPipeline->GetFinalImage();
	}

	void SceneRenderer::Flush()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::Flush");

		m_CommandBuffer->RT_Begin();

		PrepareRender();
		
		PreDepthPass();
		GeometryPass();
		CompositePass();

		m_CommandBuffer->RT_End();
	}

	void SceneRenderer::PrepareRender()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::PrepareRender");

		Renderer::SubmitCommand([]()
		{
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			Application::Get().RenderGui();

			ImGui::Render();
		});
	}

	void SceneRenderer::PreDepthPass()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::PreDepthPass");

		m_TimestampQueries.PreDepthQuery = m_CommandBuffer->BeginTimestampQuery();

		// Transition depth image for writing
		Renderer::SubmitCommand([this]()
		{
			VkCommandBuffer commandBuffer = m_CommandBuffer->GetCurrentCommandBuffer();;

			for (uint32_t i = 0; i < s_MaxLights; i++)
				Image::TransitionImage(commandBuffer, m_ShadowMaps[i]->GetImageInfo().Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		});

		// Update descriptors
		Renderer::SubmitCommand([this]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			const auto& descriptorSets = m_PreDepthPipeline->GetDescriptorSets(frameIndex);

			DescriptorWriter writer;

			// Set 1 - Scene
			const auto& buffers = m_LightsUB->GetBuffers();
			VkBuffer buffer = buffers[frameIndex];
			writer.WriteBuffer(m_LightsUB->GetBinding(), buffer, sizeof(LightsData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

			writer.UpdateSet(descriptorSets[0]);
		});

		for (uint32_t i = 0; i < m_LightsBuffer.NumLights; i++)
		{
			Renderer::SubmitCommand([this, i]()
			{
				auto& spec = m_PreDepthPipeline->GetSpecification();
				spec.DepthImage = m_ShadowMaps[i];
			});

			// Begin rendering
			Renderer::RT_BeginRenderPass(m_CommandBuffer, m_PreDepthPipeline);

			m_PreDepthPipeline->RT_Bind(m_CommandBuffer);
			m_PreDepthPipeline->RT_SetViewport(m_CommandBuffer);
			m_PreDepthPipeline->RT_SetScissor(m_CommandBuffer);
			m_PreDepthPipeline->RT_BindDescriptorSets(m_CommandBuffer, 0, 2);

			// Render geometry
			for (auto& dc : m_DrawList)
			{
				m_RenderStatistics.MeshInstances++;

				for (const auto& submesh : dc.Mesh->GetSubmeshes())
				{
					submesh.RT_BindVertexBuffer(m_CommandBuffer);
					submesh.RT_BindIndexBuffer(m_CommandBuffer);

					glm::mat4 finalTransform = dc.Transform * submesh.GetLocalTransform();

					for (const auto& p : submesh.GetPrimitives())
					{
						const auto& spec = m_PreDepthPipeline->GetSpecification();

						Buffer buffer(spec.PushConstantRanges[0].size);
						buffer.SetData(finalTransform);
						buffer.SetData(i, 64);

						m_RenderStatistics.DrawCalls++;
						m_PreDepthPipeline->RT_SetPushConstants(m_CommandBuffer, buffer);
						m_PreDepthPipeline->RT_Draw(m_CommandBuffer, p);
					}
				}
			}

			// End rendering
			Renderer::RT_EndRenderPass(m_CommandBuffer);
		}

		// Transition image for reading
		Renderer::SubmitCommand([this]()
		{
			VkCommandBuffer commandBuffer = m_CommandBuffer->GetCurrentCommandBuffer();

			for (uint32_t i = 0; i < s_MaxLights; i++)
				Image::TransitionImage(commandBuffer, m_ShadowMaps[i]->GetImageInfo().Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
		});

		 m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.PreDepthQuery);
	}

	void SceneRenderer::GeometryPass()
	{
		EPPO_PROFILE_FUNCTION("SceneRenderer::GeometryPass");

		m_TimestampQueries.GeometryQuery = m_CommandBuffer->BeginTimestampQuery();

		// Update descriptors
		Renderer::SubmitCommand([this]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			const auto& descriptorSets = m_GeometryPipeline->GetDescriptorSets(frameIndex);

			DescriptorWriter writer;

			// Set 1 - Scene
			{
				const auto& buffers = m_CameraUB->GetBuffers();
				VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_CameraUB->GetBinding(), buffer, sizeof(CameraData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			{
				const auto& buffers = m_LightsUB->GetBuffers();
				VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_LightsUB->GetBinding(), buffer, sizeof(LightsData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			std::vector<VkDescriptorImageInfo> imageInfos;

			for (const auto& shadowMap : m_ShadowMaps)
			{
				const ImageInfo& imageInfo = shadowMap->GetImageInfo();

				VkDescriptorImageInfo& info = imageInfos.emplace_back();
				info.imageLayout = imageInfo.ImageLayout;
				info.imageView = imageInfo.ImageView;
				info.sampler = imageInfo.Sampler;
			}

			writer.WriteImages(2, imageInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			writer.UpdateSet(descriptorSets[0]);
			writer.Clear();

			// Set 2 - Mesh
			imageInfos.clear();

			for (const auto& dc : m_DrawList)
			{
				for (const auto& image : dc.Mesh->GetImages())
				{
					const ImageInfo& imageInfo = image->GetImageInfo();

					VkDescriptorImageInfo& info = imageInfos.emplace_back();
					info.imageLayout = imageInfo.ImageLayout;
					info.imageView = imageInfo.ImageView;
					info.sampler = imageInfo.Sampler;
				}
			}

			// TODO:  For multiple meshes we can just up the texture index with the mesh material count?
			writer.WriteImages(0, imageInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			writer.UpdateSet(descriptorSets[1]);
		});

		// Begin rendering
		Renderer::RT_BeginRenderPass(m_CommandBuffer, m_GeometryPipeline);

		const auto& spec = m_GeometryPipeline->GetSpecification();

		m_GeometryPipeline->RT_Bind(m_CommandBuffer);
		m_GeometryPipeline->RT_SetViewport(m_CommandBuffer);
		m_GeometryPipeline->RT_SetScissor(m_CommandBuffer);
		m_GeometryPipeline->RT_BindDescriptorSets(m_CommandBuffer, 0, 2);

		// Render geometry
		for (auto& dc : m_DrawList)
		{
			m_RenderStatistics.Meshes++;
			m_RenderStatistics.MeshInstances++;

			for (const auto& submesh : dc.Mesh->GetSubmeshes())
			{
				m_RenderStatistics.Submeshes++;
				
				submesh.RT_BindVertexBuffer(m_CommandBuffer);
				submesh.RT_BindIndexBuffer(m_CommandBuffer);

				glm::mat4 finalTransform = dc.Transform * submesh.GetLocalTransform();

				for (const auto& p : submesh.GetPrimitives())
				{
					Buffer buffer(spec.PushConstantRanges[0].size);
					buffer.SetData(finalTransform);
					buffer.SetData(p.Material->DiffuseMapIndex, 64);
					buffer.SetData(p.Material->DiffuseMapIndex, 68);
					buffer.SetData(p.Material->DiffuseMapIndex, 72);

					m_RenderStatistics.DrawCalls++;
					m_GeometryPipeline->RT_SetPushConstants(m_CommandBuffer, buffer);
					m_GeometryPipeline->RT_Draw(m_CommandBuffer, p);
				}
			}
		}

		// End rendering
		Renderer::RT_EndRenderPass(m_CommandBuffer);

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.GeometryQuery);
	}

	void SceneRenderer::CompositePass()
	{
		m_TimestampQueries.CompositeQuery = m_CommandBuffer->BeginTimestampQuery();

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = m_CommandBuffer->GetCurrentCommandBuffer();

			Image::TransitionImage(commandBuffer, swapchain->GetCurrentImage(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		});

		Renderer::RT_BeginRenderPass(m_CommandBuffer, m_CompositePipeline);

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = m_CommandBuffer->GetCurrentCommandBuffer();

			ImDrawData* data = ImGui::GetDrawData();
			ImGui_ImplVulkan_RenderDrawData(data, commandBuffer);

			ImGuiIO& io = ImGui::GetIO();

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backupContext = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backupContext);
			}
		});

		Renderer::RT_EndRenderPass(m_CommandBuffer);

		Renderer::SubmitCommand([this]()
		{
			Ref<RendererContext> context = RendererContext::Get();
			Ref<Swapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = m_CommandBuffer->GetCurrentCommandBuffer();

			Image::TransitionImage(commandBuffer, swapchain->GetCurrentImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		});

		m_CommandBuffer->EndTimestampQuery(m_TimestampQueries.CompositeQuery);
	}
}
