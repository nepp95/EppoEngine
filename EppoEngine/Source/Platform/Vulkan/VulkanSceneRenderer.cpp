#include "pch.h"
#include "VulkanSceneRenderer.h"

#include "Core/Application.h"
#include "ImGui/Image.h";
#include "Platform/Vulkan/DescriptorWriter.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

namespace Eppo
{
	VulkanSceneRenderer::VulkanSceneRenderer(Ref<Scene> scene, const RenderSpecification& renderSpec)
		: m_RenderSpecification(renderSpec), m_Scene(scene)
	{
		Ref<VulkanContext> context = VulkanContext::Get();
		Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
		
		m_CommandBuffer = swapchain->GetCommandBuffer();

		// PreDepth
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.DepthTesting = true;
			pipelineSpec.DepthCubeMapImage = true;
			pipelineSpec.Width = 1024;
			pipelineSpec.Height = 1024;
			pipelineSpec.Shader = Renderer::GetShader("predepth");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" },
				{ ShaderDataType::Float4, "inColor" }
			};

			m_PreDepthPipeline = Pipeline::Create(pipelineSpec);

			for (uint32_t i = 0; i < s_MaxLights; i++)
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::Depth;
				imageSpec.Width = 1024;
				imageSpec.Height = 1024;
				imageSpec.CubeMap = true;

				m_ShadowMaps[i] = Image::Create(imageSpec);

				VkCommandBuffer cmd = context->GetLogicalDevice()->GetCommandBuffer(true);
				VulkanImage::TransitionImage(cmd, std::static_pointer_cast<VulkanImage>(m_ShadowMaps[i])->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
				context->GetLogicalDevice()->FlushCommandBuffer(cmd);
			}
		}

		// Geometry
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.ColorAttachments = {
				{ ImageFormat::RGBA8, true, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
				{ ImageFormat::RGBA8, false, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) },
				{ ImageFormat::RGBA8, false, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) }
			};
			pipelineSpec.DepthTesting = true;
			pipelineSpec.Width = m_RenderSpecification.Width;
			pipelineSpec.Height = m_RenderSpecification.Height;
			pipelineSpec.Shader = Renderer::GetShader("geometry");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" },
				{ ShaderDataType::Float4, "inColor" }
			};

			m_GeometryPipeline = Pipeline::Create(pipelineSpec);
		}

		// Debug Line
		if (m_RenderSpecification.DebugRendering)
		{
			Ref<Image> dstImage = m_GeometryPipeline->GetFinalImage();

			PipelineSpecification pipelineSpec;
			pipelineSpec.ColorAttachments = {
				{ ImageFormat::RGBA8, false }
			};
			pipelineSpec.ExistingImage = dstImage;
			pipelineSpec.Topology = PrimitiveTopology::Lines;
			pipelineSpec.PolygonMode = PolygonMode::Line;
			pipelineSpec.Width = dstImage->GetWidth();
			pipelineSpec.Height = dstImage->GetHeight();
			pipelineSpec.Shader = Renderer::GetShader("debug");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float4, "inColor" }
			};

			m_DebugLinePipeline = Pipeline::Create(pipelineSpec);
		}

		// Composite
		{
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();

			PipelineSpecification pipelineSpec;
			pipelineSpec.ColorAttachments = {
				{ ImageFormat::RGBA8, true, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) }
			};
			pipelineSpec.SwapchainTarget = true;
			pipelineSpec.Width = swapchain->GetWidth();
			pipelineSpec.Height = swapchain->GetHeight();
			pipelineSpec.Shader = Renderer::GetShader("composite");

			m_CompositePipeline = Pipeline::Create(pipelineSpec);
		}

		// Uniform buffers
		// Set 1
		m_CameraUB = UniformBuffer::Create(sizeof(CameraData), 0);
		m_LightsUB = UniformBuffer::Create(sizeof(LightsData), 1);
	}

	void VulkanSceneRenderer::RenderGui()
	{
		ImGui::Begin("Performance");

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

		auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);

		ImGui::Text("GPU Time: %.3fms", cmd->GetTimestamp(frameIndex));
		ImGui::Text("PreDepth Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.PreDepthQuery));
		ImGui::Text("Geometry Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.GeometryQuery));

		if (m_RenderSpecification.DebugRendering)
			ImGui::Text("Debug Line Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.DebugLineQuery));

		ImGui::Text("Composite Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.CompositeQuery));

		ImGui::Separator();

		const auto& pipelineStats = cmd->GetPipelineStatistics(frameIndex);

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

		ImGui::Begin("Debug Maps");

		{
			Ref<Image> image = m_GeometryPipeline->GetImage(1);
			float height = (static_cast<float>(image->GetHeight()) / static_cast<float>(image->GetWidth())) * 300;
			UI::Image(m_GeometryPipeline->GetImage(1), ImVec2(300.0f, height), ImVec2(0, 1), ImVec2(1, 0));
		}

		{
			Ref<Image> image = m_GeometryPipeline->GetImage(2);
			float height = (static_cast<float>(image->GetHeight()) / static_cast<float>(image->GetWidth())) * 300;
			UI::Image(m_GeometryPipeline->GetImage(2), ImVec2(300.0f, height), ImVec2(0, 1), ImVec2(1, 0));
		}

		ImGui::End();
	}

	void VulkanSceneRenderer::Resize(uint32_t width, uint32_t height)
	{
		//m_GeometryFramebuffer->Resize(width, height);
	}

	void VulkanSceneRenderer::BeginScene(const EditorCamera& editorCamera)
	{
		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraBuffer.Position = glm::vec4(editorCamera.GetPosition(), 0.0f);
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		BeginSceneInternal();
	}

	void VulkanSceneRenderer::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		// Camera UB
		m_CameraBuffer.View = glm::inverse(transform);
		m_CameraBuffer.Projection = camera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
		m_CameraBuffer.Position = transform[3];
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));

		BeginSceneInternal();
	}

	void VulkanSceneRenderer::EndScene()
	{
		Flush();
	}

	void VulkanSceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh> mesh, EntityHandle entityId)
	{
		auto& drawCommand = m_DrawList.emplace_back();
		drawCommand.Handle = entityId;
		drawCommand.Mesh = mesh;
		drawCommand.Transform = transform;
	}

	Ref<Image> VulkanSceneRenderer::GetFinalImage()
	{
		return m_GeometryPipeline->GetFinalImage();
	}

	void VulkanSceneRenderer::BeginSceneInternal()
	{
		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Lights UB
		m_LightsBuffer.Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
		//m_LightsBuffer.Projection[1][1] *= -1.0f;
		m_LightsBuffer.NumLights = 2;

		std::vector<LineVertex> lineVertices;
		std::vector<uint32_t> lineIndices;
		uint32_t vertexCount = 0;
		for (uint32_t i = 0; i < m_LightsBuffer.NumLights; i++)
		{
			// Setup Lights
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

			// Setup Debug Lines
			LineVertex& p0 = lineVertices.emplace_back();
			p0.Position = position - glm::vec3(0.0f, 0.5f, 0.0f);
			p0.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount++);

			LineVertex& p1 = lineVertices.emplace_back();
			p1.Position = position + glm::vec3(0.0f, 0.5f, 0.0f);
			p1.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount++);

			LineVertex& p2 = lineVertices.emplace_back();
			p2.Position = position - glm::vec3(0.5f, 0.0f, 0.0f);
			p2.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount++);

			LineVertex& p3 = lineVertices.emplace_back();
			p3.Position = position + glm::vec3(0.5f, 0.0f, 0.0f);
			p3.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount++);

			LineVertex& p4 = lineVertices.emplace_back();
			p4.Position = position - glm::vec3(0.0f, 0.0f, 0.5f);
			p4.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount++);

			LineVertex& p5 = lineVertices.emplace_back();
			p5.Position = position + glm::vec3(0.0f, 0.0f, 0.5f);
			p5.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount++);
		}

		m_DebugLineVertexBuffer = VertexBuffer::Create((void*)lineVertices.data(), lineVertices.size() * sizeof(LineVertex));
		m_DebugLineIndexBuffer = IndexBuffer::Create((void*)lineIndices.data(), lineIndices.size() * sizeof(uint32_t));

		m_LightsUB->SetData(&m_LightsBuffer, sizeof(LightsData));

		// Cleanup from last draw
		m_DrawList.clear();
	}

	void VulkanSceneRenderer::Flush()
	{
		m_CommandBuffer->RT_Begin();

		// Prepare buffers
		PrepareRender();

		// Record render commands
		PreDepthPass();
		GeometryPass();

		if (m_RenderSpecification.DebugRendering)
			DebugLinePass();

		CompositePass();

		// Submit work
		m_CommandBuffer->RT_End();
	}

	void VulkanSceneRenderer::PrepareRender() const
{
		Renderer::SubmitCommand([]()
		{
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			Application::Get().RenderGui();

			ImGui::Render();
		});
	}

	void VulkanSceneRenderer::PreDepthPass()
	{
		auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_PreDepthPipeline);

		m_TimestampQueries.PreDepthQuery = cmd->RT_BeginTimestampQuery();

		if (m_RenderSpecification.DebugRendering)
		{
			Renderer::SubmitCommand([cmd]()
			{
				VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

				VkDebugUtilsLabelEXT debugLabel{};
				debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				debugLabel.pLabelName = "PreDepthPass";
				debugLabel.color[0] = 0.5f;
				debugLabel.color[1] = 0.5f;
				debugLabel.color[2] = 0.0f;
				debugLabel.color[3] = 1.0f;

				vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
			});
		}

		// Transition depth image for writing
		Renderer::SubmitCommand([this, cmd]()
		{
			VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

			for (uint32_t i = 0; i < s_MaxLights; i++)
				VulkanImage::TransitionImage(commandBuffer, std::static_pointer_cast<VulkanImage>(m_ShadowMaps[i])->GetImageInfo().Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		});

		// Update descriptors
		Renderer::SubmitCommand([this, pipeline]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			const auto& descriptorSets = pipeline->GetDescriptorSets(frameIndex);

			DescriptorWriter writer;

			// Set 1 - Scene
			const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_LightsUB)->GetBuffers();
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

			pipeline->RT_Bind(m_CommandBuffer);
			pipeline->RT_SetViewport(m_CommandBuffer);
			pipeline->RT_SetScissor(m_CommandBuffer);
			pipeline->RT_BindDescriptorSets(m_CommandBuffer, 0, 2);

			// Render geometry
			for (const auto& dc : m_DrawList)
			{
				m_RenderStatistics.MeshInstances++;

				for (const auto& submesh : dc.Mesh->GetSubmeshes())
				{
					submesh.RT_BindVertexBuffer(m_CommandBuffer);
					submesh.RT_BindIndexBuffer(m_CommandBuffer);

					glm::mat4 finalTransform = dc.Transform * submesh.GetLocalTransform();

					for (const auto& p : submesh.GetPrimitives())
					{
						const auto& shader = m_PreDepthPipeline->GetSpecification().Shader;
						const auto& pcr = std::static_pointer_cast<VulkanShader>(shader)->GetPushConstantRanges();

						Buffer buffer(pcr[0].size);
						buffer.SetData(finalTransform);
						buffer.SetData(i, 64);

						m_RenderStatistics.DrawCalls++;
						pipeline->RT_SetPushConstants(m_CommandBuffer, buffer);
						pipeline->RT_DrawIndexed(m_CommandBuffer, p);
					}
				}
			}

			// End rendering
			Renderer::RT_EndRenderPass(m_CommandBuffer);
		}

		// Transition image for reading
		Renderer::SubmitCommand([this, cmd]()
		{
			VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

			for (uint32_t i = 0; i < s_MaxLights; i++)
				VulkanImage::TransitionImage(commandBuffer, std::static_pointer_cast<VulkanImage>(m_ShadowMaps[i])->GetImageInfo().Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

			if (m_RenderSpecification.DebugRendering)
			{
				vkCmdEndDebugUtilsLabelEXT(commandBuffer);
			}
		});

		cmd->RT_EndTimestampQuery(m_TimestampQueries.PreDepthQuery);
	}

	void VulkanSceneRenderer::GeometryPass()
	{
		auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_GeometryPipeline);

		m_TimestampQueries.GeometryQuery = cmd->RT_BeginTimestampQuery();

		// Update descriptors
		Renderer::SubmitCommand([this, pipeline]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			const auto& descriptorSets = pipeline->GetDescriptorSets(frameIndex);

			DescriptorWriter writer;

			// Set 1 - Scene
			{
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_CameraUB)->GetBuffers();
				VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_CameraUB->GetBinding(), buffer, sizeof(CameraData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			{
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_LightsUB)->GetBuffers();
				VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_LightsUB->GetBinding(), buffer, sizeof(LightsData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			std::vector<VkDescriptorImageInfo> imageInfos;

			for (const auto& shadowMap : m_ShadowMaps)
			{
				const ImageInfo& imageInfo = std::static_pointer_cast<VulkanImage>(shadowMap)->GetImageInfo();

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
					const ImageInfo& imageInfo = std::static_pointer_cast<VulkanImage>(image)->GetImageInfo();

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

		pipeline->RT_Bind(m_CommandBuffer);
		pipeline->RT_SetViewport(m_CommandBuffer);
		pipeline->RT_SetScissor(m_CommandBuffer);
		pipeline->RT_BindDescriptorSets(m_CommandBuffer, 0, 2);

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
					const auto& shader = pipeline->GetSpecification().Shader;
					const auto& pcr = std::static_pointer_cast<VulkanShader>(shader)->GetPushConstantRanges();

					Buffer buffer(pcr[0].size);
					buffer.SetData(finalTransform);
					buffer.SetData(p.Material->DiffuseMapIndex, 64);
					buffer.SetData(p.Material->DiffuseMapIndex, 68);
					buffer.SetData(p.Material->DiffuseMapIndex, 72);

					m_RenderStatistics.DrawCalls++;
					pipeline->RT_SetPushConstants(m_CommandBuffer, buffer);
					pipeline->RT_DrawIndexed(m_CommandBuffer, p);
				}
			}
		}

		// End rendering
		Renderer::RT_EndRenderPass(m_CommandBuffer);

		cmd->RT_EndTimestampQuery(m_TimestampQueries.GeometryQuery);
	}

	void VulkanSceneRenderer::DebugLinePass()
	{
		auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_DebugLinePipeline);
	
		m_TimestampQueries.DebugLineQuery = cmd->RT_BeginTimestampQuery();

		// Update descriptors
		Renderer::SubmitCommand([this, pipeline]()
		{
			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			const auto& descriptorSets = pipeline->GetDescriptorSets(frameIndex);

			DescriptorWriter writer;

			// Set 1 - Scene
			{
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_CameraUB)->GetBuffers();
				VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_CameraUB->GetBinding(), buffer, sizeof(CameraData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			{
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_LightsUB)->GetBuffers();
				VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_LightsUB->GetBinding(), buffer, sizeof(LightsData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			writer.UpdateSet(descriptorSets[0]);
		});

		Renderer::RT_BeginRenderPass(m_CommandBuffer, m_DebugLinePipeline);

		pipeline->RT_Bind(m_CommandBuffer);
		pipeline->RT_SetViewport(m_CommandBuffer);
		pipeline->RT_SetScissor(m_CommandBuffer);
		pipeline->RT_BindDescriptorSets(m_CommandBuffer, 0, 1);

		// Lights
		std::static_pointer_cast<VulkanVertexBuffer>(m_DebugLineVertexBuffer)->RT_Bind(cmd);
		std::static_pointer_cast<VulkanIndexBuffer>(m_DebugLineIndexBuffer)->RT_Bind(cmd);
		pipeline->RT_DrawIndexed(m_CommandBuffer, m_DebugLineIndexBuffer->GetIndexCount());
		m_RenderStatistics.DrawCalls++;

		Renderer::RT_EndRenderPass(m_CommandBuffer);

		cmd->RT_EndTimestampQuery(m_TimestampQueries.DebugLineQuery);
	}

	void VulkanSceneRenderer::CompositePass()
	{
		auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_CompositePipeline);

		m_TimestampQueries.CompositeQuery = cmd->RT_BeginTimestampQuery();

		Renderer::SubmitCommand([cmd]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

			VulkanImage::TransitionImage(commandBuffer, swapchain->GetCurrentImage(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		});

		Renderer::RT_BeginRenderPass(m_CommandBuffer, m_CompositePipeline);

		Renderer::SubmitCommand([cmd]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

			ImDrawData* data = ImGui::GetDrawData();
			ImGui_ImplVulkan_RenderDrawData(data, commandBuffer);

			const ImGuiIO& io = ImGui::GetIO();

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backupContext = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backupContext);
			}
		});

		Renderer::RT_EndRenderPass(m_CommandBuffer);

		Renderer::SubmitCommand([cmd]()
		{
			Ref<VulkanContext> context = VulkanContext::Get();
			Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

			VulkanImage::TransitionImage(commandBuffer, swapchain->GetCurrentImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		});

		cmd->RT_EndTimestampQuery(m_TimestampQueries.CompositeQuery);
	}

}
