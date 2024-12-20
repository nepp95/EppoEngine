#include "pch.h"
#include "VulkanSceneRenderer.h"

#include "Core/Application.h"
#include "ImGui/Image.h"
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
		auto renderer = context->GetRenderer();
		
		m_CommandBuffer = swapchain->GetCommandBuffer();
		m_DebugRenderer = DebugRenderer::Create();

		VkCommandBuffer cmd = context->GetLogicalDevice()->GetCommandBuffer(true);

		// PreDepth
		{
			for (uint32_t i = 0; i < s_MaxLights; i++)
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::Depth;
				imageSpec.Width = 1024;
				imageSpec.Height = 1024;
				imageSpec.CubeMap = true;

				m_ShadowMaps[i] = Image::Create(imageSpec);

				VulkanImage::TransitionImage(cmd, std::static_pointer_cast<VulkanImage>(m_ShadowMaps[i])->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			}

			PipelineSpecification pipelineSpec;
			pipelineSpec.CreateDepthImage = true;
			pipelineSpec.TestDepth = true;
			pipelineSpec.WriteDepth = true;
			pipelineSpec.Width = 1024;
			pipelineSpec.Height = 1024;
			pipelineSpec.Shader = renderer->GetShader("predepth");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" }
			};

			m_PreDepthPipeline = Pipeline::Create(pipelineSpec);
		}

		// Environment
		{
			ImageSpecification imageSpec("Resources/Textures/Environment/HDR_040_Field.hdr");
			m_EnvironmentMap = Image::Create(imageSpec);

			ImageSpecification cubeImageSpec;
			cubeImageSpec.CubeMap = true;
			cubeImageSpec.Format = ImageFormat::RGB16;
			cubeImageSpec.Width = 512;
			cubeImageSpec.Height = 512;
			m_EnvironmentCubeMap = Image::Create(cubeImageSpec);

			VulkanImage::TransitionImage(cmd, std::static_pointer_cast<VulkanImage>(m_EnvironmentCubeMap)->GetImageInfo().Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			PipelineSpecification pipelineSpec;
			pipelineSpec.RenderAttachments = {
				RenderAttachment{ m_EnvironmentCubeMap, true, glm::vec4(0.0f) }
			};
			pipelineSpec.CubeMap = true;
			pipelineSpec.CullFrontFace = CullFrontFace::CounterClockwise;
			pipelineSpec.Width = m_EnvironmentCubeMap->GetWidth();
			pipelineSpec.Height = m_EnvironmentCubeMap->GetHeight();
			pipelineSpec.Shader = renderer->GetShader("env");

			m_EnvPipeline = Pipeline::Create(pipelineSpec);
		}

		// Geometry
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.Width = m_RenderSpecification.Width;
			imageSpec.Height = m_RenderSpecification.Height;

			PipelineSpecification pipelineSpec;
			pipelineSpec.RenderAttachments = {
				RenderAttachment{ Image::Create(imageSpec), true, glm::vec4(0.0f) },
				RenderAttachment{ Image::Create(imageSpec), true, glm::vec4(0.0f) },
				RenderAttachment{ Image::Create(imageSpec), true, glm::vec4(0.0f) }
			};
			pipelineSpec.CreateDepthImage = true;
			pipelineSpec.TestDepth = true;
			pipelineSpec.WriteDepth = true;
			pipelineSpec.Width = m_RenderSpecification.Width;
			pipelineSpec.Height = m_RenderSpecification.Height;
			pipelineSpec.Shader = renderer->GetShader("geometry");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float3, "inNormal" },
				{ ShaderDataType::Float2, "inTexCoord" }
			};

			m_GeometryPipeline = Pipeline::Create(pipelineSpec);
		}

		// Skybox
		{
			Ref<Image> depthImage = m_GeometryPipeline->GetImage(3);

			PipelineSpecification pipelineSpec;
			pipelineSpec.RenderAttachments = {
				RenderAttachment{ m_GeometryPipeline->GetFinalImage(), false, glm::vec4(0.0f) },
				RenderAttachment{ depthImage, false, 1.0f }
			};
			pipelineSpec.DepthCompareOp = DepthCompareOp::LessOrEqual;
			pipelineSpec.TestDepth = true;
			pipelineSpec.CullMode = CullMode::Front;
			pipelineSpec.Width = m_RenderSpecification.Width;
			pipelineSpec.Height = m_RenderSpecification.Height;
			pipelineSpec.Shader = renderer->GetShader("skybox");

			m_SkyboxPipeline = Pipeline::Create(pipelineSpec);
		}

		// Debug Line
		if (m_RenderSpecification.DebugRendering)
		{
			Ref<Image> dstImage = m_GeometryPipeline->GetFinalImage();

			PipelineSpecification pipelineSpec;
			pipelineSpec.RenderAttachments = {
				RenderAttachment{ dstImage, false, glm::vec4(0.0f) }
			};
			pipelineSpec.Topology = PrimitiveTopology::Lines;
			pipelineSpec.PolygonMode = PolygonMode::Line;
			pipelineSpec.Width = dstImage->GetWidth();
			pipelineSpec.Height = dstImage->GetHeight();
			pipelineSpec.Shader = renderer->GetShader("debug");
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float4, "inColor" }
			};

			m_DebugLinePipeline = Pipeline::Create(pipelineSpec);
		}

		// Composite
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.SwapchainTarget = true;
			pipelineSpec.Width = swapchain->GetWidth();
			pipelineSpec.Height = swapchain->GetHeight();
			pipelineSpec.Shader = renderer->GetShader("composite");

			m_CompositePipeline = Pipeline::Create(pipelineSpec);
		}

		// Execute image transitions
		context->GetLogicalDevice()->FlushCommandBuffer(cmd);

		// Create descriptor sets from geometry shader
		const auto geometryShader = std::static_pointer_cast<VulkanShader>(m_GeometryPipeline->GetSpecification().Shader);
		const auto& descriptorSetLayouts = geometryShader->GetDescriptorSetLayouts();
		for (uint32_t i = 0; i < VulkanConfig::MaxFramesInFlight; i++)
		{
			for (uint32_t j = 0; j < 4; j++)
				m_DescriptorSets[i][j] = static_cast<VkDescriptorSet>(renderer->AllocateDescriptor(descriptorSetLayouts[j]));
		}

		// Vertex and Index buffers
		m_DebugLineVertexBuffer = VertexBuffer::Create(sizeof(LineVertex) * 100);
		m_DebugLineIndexBuffer = IndexBuffer::Create(sizeof(uint32_t) * 100);

		// Uniform buffers
		// Set 0
		m_EnvironmentUB = UniformBuffer::Create(sizeof(EnvironmentData), 2);
		// Set 1
		m_CameraUB = UniformBuffer::Create(sizeof(CameraData), 0);
		m_LightsUB = UniformBuffer::Create(sizeof(LightsData), 1);
	}

	void VulkanSceneRenderer::RenderGui()
	{
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::RenderGui");

		ImGui::Begin("Performance");

		uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
		frameIndex = frameIndex == 0 ? 1 : 0;

		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);

		ImGui::Text("GPU Time: %.3fms", cmd->GetTimestamp(frameIndex));
		ImGui::Text("PreDepth Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.PreDepthQuery));
		ImGui::Text("Geometry Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.GeometryQuery));
		ImGui::Text("Skybox Pass: %.3fms", cmd->GetTimestamp(frameIndex, m_TimestampQueries.SkyboxQuery));

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
			const Ref<Image> image = m_GeometryPipeline->GetImage(1);
			const float height = (static_cast<float>(image->GetHeight()) / static_cast<float>(image->GetWidth())) * 300;
			UI::Image(m_GeometryPipeline->GetImage(1), ImVec2(300.0f, height), ImVec2(0, 1), ImVec2(1, 0));
		}

		{
			const Ref<Image> image = m_GeometryPipeline->GetImage(2);
			const float height = (static_cast<float>(image->GetHeight()) / static_cast<float>(image->GetWidth())) * 300;
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
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::BeginScene");

		// Cleanup from last draw
		m_DrawList.clear();

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = editorCamera.GetViewMatrix();
		m_CameraBuffer.Projection = editorCamera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = editorCamera.GetViewProjectionMatrix();
		m_CameraBuffer.Position = glm::vec4(editorCamera.GetPosition(), 0.0f);
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));
	}

	void VulkanSceneRenderer::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::BeginScene");

		// Cleanup from last draw
		m_DrawList.clear();

		// Reset statistics
		memset(&m_RenderStatistics, 0, sizeof(RenderStatistics));

		// Camera UB
		m_CameraBuffer.View = glm::inverse(transform);
		m_CameraBuffer.Projection = camera.GetProjectionMatrix();
		m_CameraBuffer.ViewProjection = camera.GetProjectionMatrix() * glm::inverse(transform);
		m_CameraBuffer.Position = transform[3];
		m_CameraUB->SetData(&m_CameraBuffer, sizeof(m_CameraBuffer));
	}

	void VulkanSceneRenderer::EndScene()
	{
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::EndScene");

		Flush();
	}

	void VulkanSceneRenderer::SubmitDrawCommand(EntityType type, Ref<DrawCommand> drawCommand)
	{
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::SubmitDrawCommand");

		if (type == EntityType::PointLight && m_DrawList[EntityType::PointLight].size() == 8)
		{
			EPPO_WARN("Trying to submit more point lights than we currently support!");
			return;
		}	

		m_DrawList[type].emplace_back(drawCommand);
	}

	Ref<Image> VulkanSceneRenderer::GetFinalImage()
	{
		return m_GeometryPipeline->GetFinalImage();
	}

	void VulkanSceneRenderer::Flush()
	{
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::Flush");

		m_CommandBuffer->RT_Begin();

		// Prepare buffers
		PrepareBuffers();
		PrepareImages();
		UpdateDescriptors();

		// Record render commands
		GuiPass();
		PreDepthPass();
		EnvPass();
		GeometryPass();
		SkyboxPass();

		if (m_RenderSpecification.DebugRendering)
			DebugLinePass();

		CompositePass();

		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto renderer = VulkanContext::Get()->GetRenderer();
		renderer->SubmitCommand([cmd]()
		{
			const auto context = VulkanContext::Get();
			const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();
			EPPO_PROFILE_GPU_END(context->GetTracyContext(), commandBuffer)
		});

		// Submit work
		m_CommandBuffer->RT_End();
	}

	void Eppo::VulkanSceneRenderer::PrepareBuffers()
	{
		EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::PrepareBuffers");

		// Environment UB
		m_EnvironmentBuffer.Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		m_EnvironmentBuffer.View[0] = glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.View[1] = glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.View[2] = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		m_EnvironmentBuffer.View[3] = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		m_EnvironmentBuffer.View[4] = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentBuffer.View[5] = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		m_EnvironmentUB->SetData(&m_EnvironmentBuffer, sizeof(EnvironmentData));

		// Lights UB
		m_LightsBuffer.Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
		m_LightsBuffer.NumLights = 0;

		std::vector<LineVertex> lineVertices;
		std::vector<uint32_t> lineIndices;
		uint32_t vertexCount = 0;

		uint32_t i = 0;
		while (i < 8 && !m_DrawList[EntityType::PointLight].empty())
		{
			Ref<DrawCommand> dc = m_DrawList[EntityType::PointLight].back();
			const auto plCmd = std::static_pointer_cast<PointLightCommand>(dc);

			auto& [view, position, color] = m_LightsBuffer.Lights[i];
			position = glm::vec4(plCmd->Position, 1.0f);
			color = plCmd->Color;
			view[0] = lookAt(plCmd->Position, plCmd->Position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			view[1] = lookAt(plCmd->Position, plCmd->Position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			view[2] = lookAt(plCmd->Position, plCmd->Position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			view[3] = lookAt(plCmd->Position, plCmd->Position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
			view[4] = lookAt(plCmd->Position, plCmd->Position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
			view[5] = lookAt(plCmd->Position, plCmd->Position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

			// Setup Debug Lines
			LineVertex& p0 = lineVertices.emplace_back();
			p0.Position = plCmd->Position - glm::vec3(0.0f, 0.5f, 0.0f);
			p0.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount);
			vertexCount++;

			LineVertex& p1 = lineVertices.emplace_back();
			p1.Position = plCmd->Position + glm::vec3(0.0f, 0.5f, 0.0f);
			p1.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount);
			vertexCount++;

			LineVertex& p2 = lineVertices.emplace_back();
			p2.Position = plCmd->Position - glm::vec3(0.5f, 0.0f, 0.0f);
			p2.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount);
			vertexCount++;

			LineVertex& p3 = lineVertices.emplace_back();
			p3.Position = plCmd->Position + glm::vec3(0.5f, 0.0f, 0.0f);
			p3.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount);
			vertexCount++;

			LineVertex& p4 = lineVertices.emplace_back();
			p4.Position = plCmd->Position - glm::vec3(0.0f, 0.0f, 0.5f);
			p4.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount);
			vertexCount++;

			LineVertex& p5 = lineVertices.emplace_back();
			p5.Position = plCmd->Position + glm::vec3(0.0f, 0.0f, 0.5f);
			p5.Color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			lineIndices.emplace_back(vertexCount);
			vertexCount++;

			m_DrawList[EntityType::PointLight].pop_back();
			i++;
		}

		m_LightsBuffer.NumLights = i;

		if (!lineVertices.empty() && !lineIndices.empty())
		{
			Buffer ib = Buffer::Copy(lineIndices.data(), sizeof(uint32_t) * lineIndices.size());
			m_DebugLineIndexBuffer->SetData(ib);
			ib.Release();

			Buffer vb = Buffer::Copy(lineVertices.data(), lineVertices.size() * sizeof(LineVertex));
			m_DebugLineVertexBuffer->SetData(vb);
			vb.Release();

			m_DebugLineCount = static_cast<uint32_t>(lineIndices.size());
		}

		m_LightsUB->SetData(&m_LightsBuffer, sizeof(LightsData));
	}

	void VulkanSceneRenderer::PrepareImages() const
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		renderer->SubmitCommand([this, cmd]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::PrepareImages");

			const auto commandBuffer = cmd->GetCurrentCommandBuffer();

			// Transition depth images for writing
			for (uint32_t i = 0; i < s_MaxLights; i++)
				VulkanImage::TransitionImage(commandBuffer, std::static_pointer_cast<VulkanImage>(m_ShadowMaps[i])->GetImageInfo().Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		});
	}

	void VulkanSceneRenderer::UpdateDescriptors()
	{
		const auto renderer = VulkanContext::Get()->GetRenderer();
		renderer->SubmitCommand([this]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::UpdateDescriptors");

			const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
			const auto descriptorSets = m_DescriptorSets[frameIndex];

			DescriptorWriter writer;

			// Set 0 - Global
			{
				// Binding 0
				const auto& info = std::static_pointer_cast<VulkanImage>(m_EnvironmentMap)->GetImageInfo();
				writer.WriteImage(0, info.ImageView, info.Sampler, info.ImageLayout, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			}

			{
				// Binding 1
				const auto& info = std::static_pointer_cast<VulkanImage>(m_EnvironmentCubeMap)->GetImageInfo();
				writer.WriteImage(1, info.ImageView, info.Sampler, info.ImageLayout, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			}

			{
				// Binding 2
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_EnvironmentUB)->GetBuffers();
				const VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_EnvironmentUB->GetBinding(), buffer, sizeof(EnvironmentData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			writer.UpdateSet(descriptorSets[0]);
			writer.Clear();

			// Set 1 - Scene
			{
				// Binding 0
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_CameraUB)->GetBuffers();
				const VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_CameraUB->GetBinding(), buffer, sizeof(CameraData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			{
				// Binding 1
				const auto& buffers = std::static_pointer_cast<VulkanUniformBuffer>(m_LightsUB)->GetBuffers();
				const VkBuffer buffer = buffers[frameIndex];
				writer.WriteBuffer(m_LightsUB->GetBinding(), buffer, sizeof(LightsData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			}

			std::vector<VkDescriptorImageInfo> imageInfos;
			{
				// Binding 2
				for (const auto& shadowMap : m_ShadowMaps)
				{
					const ImageInfo& imageInfo = std::static_pointer_cast<VulkanImage>(shadowMap)->GetImageInfo();

					auto& [sampler, imageView, imageLayout] = imageInfos.emplace_back();
					imageLayout = imageInfo.ImageLayout;
					imageView = imageInfo.ImageView;
					sampler = imageInfo.Sampler;
				}

				writer.WriteImages(2, imageInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			}

			writer.UpdateSet(descriptorSets[1]);
			writer.Clear();
			imageInfos.clear();

			// Set 2 - Material
			// TODO: When we go over the max limit, we need to do this in the pass itself
			{
				// Binding 0
				for (const auto& dc : m_DrawList[EntityType::Mesh])
				{
					const auto meshCmd = std::static_pointer_cast<MeshCommand>(dc);

					for (const auto& image : meshCmd->Mesh->GetImages())
					{
						const ImageInfo& imageInfo = std::static_pointer_cast<VulkanImage>(image)->GetImageInfo();

						auto& [sampler, imageView, imageLayout] = imageInfos.emplace_back();
						imageLayout = imageInfo.ImageLayout;
						imageView = imageInfo.ImageView;
						sampler = imageInfo.Sampler;
					}

					writer.WriteImages(0, imageInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				}
			}

			writer.UpdateSet(descriptorSets[2]);
		});
	}

	void VulkanSceneRenderer::GuiPass()
	{
		const auto renderer = VulkanContext::Get()->GetRenderer();
		renderer->SubmitCommand([]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::GuiPass");

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			Application::Get().RenderGui();

			ImGui::Render();
		});
	}

	void VulkanSceneRenderer::PreDepthPass()
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_PreDepthPipeline);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		m_TimestampQueries.PreDepthQuery = cmd->RT_BeginTimestampQuery();

		renderer->SubmitCommand([this, cmd, pipeline, renderer]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::PreDepthPass");
			
			// Get all required variables
			const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();
			auto& spec = pipeline->GetSpecification();

			const auto& pcr = std::static_pointer_cast<VulkanShader>(spec.Shader)->GetPushConstantRanges();
			ScopedBuffer pcrBuffer(pcr[0].size);
			
			// Profiling
			EPPO_PROFILE_GPU(VulkanContext::Get()->GetTracyContext(), cmd->GetCurrentCommandBuffer(), "PreDepth")

			// Insert debug label
			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->StartDebugLabel(m_CommandBuffer, "PreDepthPass");

			// Update descriptor sets
			const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
			const auto& descriptorSets = m_DescriptorSets[frameIndex];

			for (uint32_t i = 0; i < m_LightsBuffer.NumLights; i++)
			{
				spec.RenderAttachments.clear();
				spec.RenderAttachments.emplace_back(m_ShadowMaps[i], true, 1.0f);

				// Begin rendering
				renderer->BeginRenderPass(m_CommandBuffer, m_PreDepthPipeline);

				// Bind pipeline
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

				// Set viewport and scissor
				VkViewport viewport{};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(spec.Width);
				viewport.height = static_cast<float>(spec.Height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

				VkRect2D scissor{};
				scissor.offset = { 0, 0 };
				scissor.extent = { spec.Width, spec.Height };

				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

				// Bind descriptor sets
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 3, descriptorSets.data(), 0, nullptr);

				// Render geometry
				for (const auto& dc : m_DrawList[EntityType::Mesh])
				{
					const auto meshCmd = std::static_pointer_cast<MeshCommand>(dc);
					m_RenderStatistics.MeshInstances++;

					for (const auto& submesh : meshCmd->Mesh->GetSubmeshes())
					{
						// Bind vertex buffer
						const auto vertexBuffer = std::static_pointer_cast<VulkanVertexBuffer>(submesh.GetVertexBuffer());
						VkBuffer vb = { vertexBuffer->GetBuffer() };
						constexpr VkDeviceSize offsets[] = { 0 };

						vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

						// Bind index buffer
						const auto indexBuffer = std::static_pointer_cast<VulkanIndexBuffer>(submesh.GetIndexBuffer());
						vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

						// Draw call
						glm::mat4 finalTransform = meshCmd->Transform * submesh.GetLocalTransform();

						for (const auto& p : submesh.GetPrimitives())
						{
							pcrBuffer.SetData(finalTransform);
							pcrBuffer.SetData(i, 64);

							vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, pcrBuffer.Size(), pcrBuffer.Data());

							m_RenderStatistics.DrawCalls++;
							vkCmdDrawIndexed(commandBuffer, p.IndexCount, 1, p.FirstIndex, static_cast<int32_t>(p.FirstVertex), 0);
						}
					}
				}

				// End rendering
				renderer->EndRenderPass(m_CommandBuffer);
			}

			// Transition image for reading
			for (uint32_t i = 0; i < s_MaxLights; i++)
				VulkanImage::TransitionImage(commandBuffer, std::static_pointer_cast<VulkanImage>(m_ShadowMaps[i])->GetImageInfo().Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->EndDebugLabel(m_CommandBuffer);
		});

		cmd->RT_EndTimestampQuery(m_TimestampQueries.PreDepthQuery);
	}

	void VulkanSceneRenderer::EnvPass()
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_EnvPipeline);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		renderer->SubmitCommand([this, cmd, pipeline, renderer]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::EnvPass");

			const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();
			const auto& spec = pipeline->GetSpecification();

			// Update descriptor sets
			uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
			const auto& descriptorSets = m_DescriptorSets[frameIndex];

			// Begin rendering
			renderer->BeginRenderPass(m_CommandBuffer, pipeline);

			// Bind pipeline
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
		
			// Set viewport and scissor
			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(spec.Width);
			viewport.height = static_cast<float>(spec.Height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = { spec.Width, spec.Height };

			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			// Bind descriptor sets
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 3, descriptorSets.data(), 0, nullptr);
			
			// Draw call
			m_RenderStatistics.DrawCalls++;
			vkCmdDraw(commandBuffer, 36, 1, 0, 0);

			renderer->EndRenderPass(m_CommandBuffer);
		});
	}

	void VulkanSceneRenderer::SkyboxPass()
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_SkyboxPipeline);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		m_TimestampQueries.SkyboxQuery = cmd->RT_BeginTimestampQuery();

		renderer->SubmitCommand([this, cmd, pipeline, renderer]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::SkyboxPass");

			const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();
			const auto& spec = pipeline->GetSpecification();

			// Profiling
			EPPO_PROFILE_GPU(VulkanContext::Get()->GetTracyContext(), cmd->GetCurrentCommandBuffer(), "SkyboxPass")

			// Insert debug label
			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->StartDebugLabel(m_CommandBuffer, "SkyboxPass");

			// Get descriptor sets
			const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
			const auto& descriptorSets = m_DescriptorSets[frameIndex];

			// Begin rendering
			renderer->BeginRenderPass(m_CommandBuffer, pipeline);

			// Bind pipeline
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

			// Set viewport and scissor
			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(spec.Width);
			viewport.height = static_cast<float>(spec.Height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = { spec.Width, spec.Height };

			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			// Bind descriptor sets
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 3, descriptorSets.data(), 0, nullptr);

			// Draw call
			m_RenderStatistics.DrawCalls++;
			vkCmdDraw(commandBuffer, 36, 1, 0, 0);

			renderer->EndRenderPass(m_CommandBuffer);

			// End debug label
			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->EndDebugLabel(m_CommandBuffer);
		});

		cmd->RT_EndTimestampQuery(m_TimestampQueries.SkyboxQuery);
	}

	void VulkanSceneRenderer::GeometryPass()
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_GeometryPipeline);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		m_TimestampQueries.GeometryQuery = cmd->RT_BeginTimestampQuery();

		renderer->SubmitCommand([this, cmd, pipeline, renderer]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::GeometryPass");

			const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();
			const auto& spec = pipeline->GetSpecification();

			// Profiling
			EPPO_PROFILE_GPU(VulkanContext::Get()->GetTracyContext(), cmd->GetCurrentCommandBuffer(), "GeometryPass")

			// Insert debug label
			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->StartDebugLabel(m_CommandBuffer, "GeometryPass");

			// Get descriptor sets
			const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
			const auto& descriptorSets = m_DescriptorSets[frameIndex];

			// Begin rendering
			renderer->BeginRenderPass(m_CommandBuffer, pipeline);

			// Bind pipeline
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

			// Set viewport and scissor
			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(spec.Width);
			viewport.height = static_cast<float>(spec.Height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = { spec.Width, spec.Height };

			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			// Bind descriptor sets
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 3, descriptorSets.data(), 0, nullptr);
		
			// Render geometry
			for (const auto& dc : m_DrawList[EntityType::Mesh])
			{
				const auto meshCmd = std::static_pointer_cast<MeshCommand>(dc);

				m_RenderStatistics.Meshes++;
				m_RenderStatistics.MeshInstances++;

				for (const auto& submesh : meshCmd->Mesh->GetSubmeshes())
				{
					m_RenderStatistics.Submeshes++;

					// Bind vertex buffer
					const auto vertexBuffer = std::static_pointer_cast<VulkanVertexBuffer>(submesh.GetVertexBuffer());
					VkBuffer vb = { vertexBuffer->GetBuffer() };
					constexpr VkDeviceSize offsets[] = { 0 };

					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

					// Bind index buffer
					const auto indexBuffer = std::static_pointer_cast<VulkanIndexBuffer>(submesh.GetIndexBuffer());
					vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

					// Draw call
					glm::mat4 finalTransform = meshCmd->Transform * submesh.GetLocalTransform();

					for (const auto& p : submesh.GetPrimitives())
					{
						const auto& shader = pipeline->GetSpecification().Shader;
						const auto& pcr = std::static_pointer_cast<VulkanShader>(shader)->GetPushConstantRanges();

						ScopedBuffer buffer(pcr[0].size);
						buffer.SetData(finalTransform);
						buffer.SetData(p.Material->DiffuseColor, 64);
						buffer.SetData(p.Material->DiffuseMapIndex, 80);
						buffer.SetData(p.Material->NormalMapIndex, 84);
						buffer.SetData(p.Material->RoughnessMetallicMapIndex, 88);

						vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, buffer.Size(), buffer.Data());
						
						m_RenderStatistics.DrawCalls++;
						vkCmdDrawIndexed(commandBuffer, p.IndexCount, 1, p.FirstIndex, static_cast<int32_t>(p.FirstVertex), 0);
					}
				}
			}

			// End rendering
			renderer->EndRenderPass(m_CommandBuffer);

			// End debug label
			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->EndDebugLabel(m_CommandBuffer);
		});

		cmd->RT_EndTimestampQuery(m_TimestampQueries.GeometryQuery);
	}

	void VulkanSceneRenderer::DebugLinePass()
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_DebugLinePipeline);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		m_TimestampQueries.DebugLineQuery = cmd->RT_BeginTimestampQuery();

		if (m_LightsBuffer.NumLights > 0)
		{
			renderer->SubmitCommand([this, cmd, pipeline, renderer]()
			{
				EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::DebugLinePass");

				const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();
				const auto& spec = pipeline->GetSpecification();
		
				// Profiling
				EPPO_PROFILE_GPU(VulkanContext::Get()->GetTracyContext(), cmd->GetCurrentCommandBuffer(), "DebugLinePass")

				// Insert debug label
				if (m_RenderSpecification.DebugRendering)
					m_DebugRenderer->StartDebugLabel(m_CommandBuffer, "DebugLinePass");
		
				// Update descriptors
				const uint32_t frameIndex = VulkanContext::Get()->GetCurrentFrameIndex();
				const auto& descriptorSets = m_DescriptorSets[frameIndex];
		
				// Begin rendering
				renderer->BeginRenderPass(m_CommandBuffer, m_DebugLinePipeline);

				// Bind pipeline
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());

				// Set viewport and scissor
				VkViewport viewport;
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(spec.Width);
				viewport.height = static_cast<float>(spec.Height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

				VkRect2D scissor;
				scissor.offset = { 0, 0 };
				scissor.extent = { spec.Width, spec.Height };

				vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

				// Bind descriptor sets
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 3, descriptorSets.data(), 0, nullptr);
		
				// Bind vertex buffer
				const auto vertexBuffer = std::static_pointer_cast<VulkanVertexBuffer>(m_DebugLineVertexBuffer);
				const VkBuffer vb = { vertexBuffer->GetBuffer() };
				constexpr VkDeviceSize offsets[] = { 0 };

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);

				// Bind index buffer
				const auto indexBuffer = std::static_pointer_cast<VulkanIndexBuffer>(m_DebugLineIndexBuffer);
				vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
		
				// Draw call
				m_RenderStatistics.DrawCalls++;
				vkCmdDrawIndexed(commandBuffer, m_DebugLineCount, 1, 0, 0, 0);
		
				// End rendering
				renderer->EndRenderPass(m_CommandBuffer);

				if (m_RenderSpecification.DebugRendering)
					m_DebugRenderer->EndDebugLabel(m_CommandBuffer);
			});
		}

		cmd->RT_EndTimestampQuery(m_TimestampQueries.DebugLineQuery);
	}

	void VulkanSceneRenderer::CompositePass()
	{
		const auto cmd = std::static_pointer_cast<VulkanCommandBuffer>(m_CommandBuffer);
		const auto pipeline = std::static_pointer_cast<VulkanPipeline>(m_CompositePipeline);
		const auto renderer = VulkanContext::Get()->GetRenderer();

		m_TimestampQueries.CompositeQuery = cmd->RT_BeginTimestampQuery();

		renderer->SubmitCommand([this, cmd, pipeline, renderer]()
		{
			EPPO_PROFILE_FUNCTION("VulkanSceneRenderer::CompositePass");

			const Ref<VulkanContext> context = VulkanContext::Get();
			const Ref<VulkanSwapchain> swapchain = context->GetSwapchain();
			const VkCommandBuffer commandBuffer = cmd->GetCurrentCommandBuffer();

			// Profiling
			EPPO_PROFILE_GPU(VulkanContext::Get()->GetTracyContext(), cmd->GetCurrentCommandBuffer(), "CompositePass")

			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->StartDebugLabel(cmd, "CompositePass");

			VulkanImage::TransitionImage(commandBuffer, swapchain->GetCurrentImage(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			renderer->BeginRenderPass(cmd, pipeline);

			ImDrawData* data = ImGui::GetDrawData();
			ImGui_ImplVulkan_RenderDrawData(data, commandBuffer);

			if (const ImGuiIO& io = ImGui::GetIO();
				io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backupContext = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backupContext);
			}

			renderer->EndRenderPass(cmd);

			VulkanImage::TransitionImage(commandBuffer, swapchain->GetCurrentImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

			if (m_RenderSpecification.DebugRendering)
				m_DebugRenderer->EndDebugLabel(cmd);
		});

		cmd->RT_EndTimestampQuery(m_TimestampQueries.CompositeQuery);
	}
}
