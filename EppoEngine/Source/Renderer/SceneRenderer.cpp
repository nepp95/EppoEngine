#include "pch.h"
#include "Renderer/SceneRenderer.h"

#include "Core/Application.h"
#include "Project/Project.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/Vertex.h"

#include <nvrhi/utils.h>

namespace Eppo
{
	SceneRenderer::SceneRenderer(const Ref<Scene>& scene, uint32_t width, uint32_t height)
		: m_Scene(scene), m_Width(width), m_Height(height)
	{
		EP_PROFILE_FN("SceneRenderer::SceneRenderer")

		const auto& dm = DeviceManager::Get();
		auto device = dm->GetDevice();
		const auto& renderer = dm->GetRenderer();

		m_CommandList = device->createCommandList();

		uint32_t maxFrames = dm->GetParams().MaxFramesInFlight;
		m_TimerQueries.resize(maxFrames);
		m_LastQueryTimes.resize(maxFrames);

		for (uint32_t i = 0; i < maxFrames; i++)
			m_TimerQueries[i] = device->createTimerQuery();

		if (m_Width == 0 || m_Height == 0)
		{
			const auto& app = Application::Get();
			m_Width = app.GetWindow()->GetWidth();
			m_Height = app.GetWindow()->GetHeight();
		}

		nvrhi::SamplerDesc samplerDesc{};
		samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
		samplerDesc.setAllFilters(true);
		m_Sampler = device->createSampler(samplerDesc);

		// Geometry Pipeline
		{
			FramebufferSpecification framebufferSpec{
				.Width = m_Width,
				.Height = m_Height,
				.Attachments = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::D32 },
				.ClearColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
				.ClearColorOnLoad = true,
				.ClearDepthOnLoad = true,
				.DebugName = "Framebuffer Geometry",
			};
			
			PipelineSpecification pipelineSpec{
				.Shader = renderer->GetShader("geometry"),
				.Framebuffer = CreateRef<Framebuffer>(framebufferSpec),
				.Width = m_Width,
				.Height = m_Height,
				.CullMode = nvrhi::RasterCullMode::Front,
				.DepthTestEnable = true,
				.DepthWriteEnable = true,
			};

			m_GeometryPipeline = CreateRef<Pipeline>(pipelineSpec);
		}

		// Uniform buffers
		m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Camera");
		m_LightsUB = CreateRef<UniformBuffer>(sizeof(LightData) * 4, "UniformBuffer Lights");
	
		constexpr float p = 15.0f;
		m_LightData[0] = { glm::vec3(-5.0f, 1.0f, -5.0f), 0.0f, glm::vec4(-p, -p * 0.5f, -p, 1.0f) };
		m_LightData[1] = { glm::vec3(-5.0f, 1.0f, 5.0f), 0.0f, glm::vec4(-p, -p * 0.5f, p, 1.0f) };
		m_LightData[2] = { glm::vec3(5.0f, 1.0f, -5.0f), 0.0f, glm::vec4(p, -p * 0.5f, p, 1.0f) };
		m_LightData[3] = { glm::vec3(5.0f, 1.0f, 5.0f), 0.0f, glm::vec4(p, -p * 0.5f, -p, 1.0f) };
		m_LightsUB->SetData(&m_LightData, sizeof(LightData) * 4);
	}

	auto SceneRenderer::RenderGui() const -> void
	{
		EP_PROFILE_FN("SceneRenderer::RenderGui")

		const auto& app = Application::Get();
		const auto& dm = DeviceManager::Get();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		ImGui::Begin("Scene Renderer");
		ImGui::SeparatorText("Draw Statistics");
		ImGui::Text("Draw calls: %u", m_DrawStatistics.DrawCalls);
		ImGui::Text("Instances: %u", m_DrawStatistics.Instances);
		ImGui::Text("Meshes: %u", m_DrawStatistics.Meshes);
		ImGui::Text("Submeshes: %u", m_DrawStatistics.Submeshes);
		ImGui::SeparatorText("Render Passes");
		ImGui::Text("UI: %.2fms", app.GetImGuiLayer()->GetMainImGuiRenderer()->GetGPUTime(frameIndex));
		ImGui::Text("Geometry: %.2fms", m_LastQueryTimes.at(frameIndex));
		ImGui::End();
	}

	auto SceneRenderer::BeginScene(const ScopedPtr<EditorCamera>& camera) -> void
	{
		BeginScene(camera->GetViewMatrix(), camera->GetProjectionMatrix(), camera->GetPosition());
	}

	auto SceneRenderer::BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position) -> void
	{
		EP_PROFILE_FN("SceneRenderer::BeginScene")

		// Reset
		m_DrawCommands.clear();
		std::memset(&m_DrawStatistics, 0, sizeof(DrawStatistics));

		// Set uniforms
		m_CameraData.View = view;
		m_CameraData.Projection = projection;
		m_CameraData.ViewProjection = projection * view;
		m_CameraData.Position = glm::vec4(position, 0.0f);
		m_CameraUB->SetData(&m_CameraData, sizeof(CameraData));
	}

	auto SceneRenderer::EndScene() -> void
	{
		EP_PROFILE_FN("SceneRenderer::EndScene")

		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		// Update descriptor table
		const auto& descriptorTable = m_GeometryPipeline->GetSpecification().Shader->GetDescriptorTable();

		// Resize descriptor table
		uint32_t imageCount = 0;
		for (const auto& [key, drawCmd] : m_DrawCommands)
			imageCount += drawCmd.ImageCount;

		device->resizeDescriptorTable(descriptorTable, imageCount, false);

		// Write descriptor table and gather instance transforms
		uint32_t imageOffset = 0;
		std::vector<glm::mat4> instanceTransforms;

		for (auto& [key, drawCmd] : m_DrawCommands)
		{
			// Mesh textures
			const auto& images = drawCmd.Mesh->GetImages();

			for (uint32_t i = 0; i < images.size(); i++)
			{
				const auto& image = images.at(i);
				device->writeDescriptorTable(descriptorTable, nvrhi::BindingSetItem::Texture_SRV(i + imageOffset, image->GetTexture(), image->GetFormat()));
			}
			
			drawCmd.ImageOffset = imageOffset;
			imageOffset += static_cast<uint32_t>(images.size());

			// Mesh instance transforms
			drawCmd.InstanceOffset = static_cast<uint32_t>(instanceTransforms.size());
			instanceTransforms.insert(instanceTransforms.end(), drawCmd.Transforms.begin(), drawCmd.Transforms.end());
		}

		// Reallocate instance transforms buffer if needed
		const uint64_t requiredSize = instanceTransforms.size() * sizeof(glm::mat4);

		if (!m_InstanceTransformsSB)
			m_InstanceTransformsSB = CreateRef<StorageBuffer>(static_cast<uint32_t>(sizeof(glm::mat4)), requiredSize, "StorageBuffer Instance Transforms");

		m_InstanceTransformsSB->SetData(instanceTransforms.data(), requiredSize);

		GeometryPass();

		m_LastQueryTimes[frameIndex] = device->getTimerQueryTime(m_TimerQueries.at(frameIndex)) * 1000.0f;
		device->resetTimerQuery(m_TimerQueries.at(frameIndex));
	}

	auto SceneRenderer::GetFinalImage() const -> const Ref<Image>&
	{
		return m_GeometryPipeline->GetSpecification().Framebuffer->GetFinalImage();
	}

	auto SceneRenderer::SubmitMesh(const AssetHandle meshHandle, const glm::mat4& transform) -> void
	{
		const DrawKey key{
			.ID = meshHandle,
		};

		if (m_DrawCommands.contains(key))
		{
			auto& drawCmd = m_DrawCommands.at(key);
			drawCmd.Transforms.emplace_back(transform);
		}
		else
		{
			const auto& mesh = Project::GetActive()->GetAssetManager()->GetOrLoadAsset<Mesh>(meshHandle);

			const DrawCommand cmd{
				.Mesh = mesh,
				.Transforms = { transform },
				.ImageCount = static_cast<uint32_t>(mesh->GetImages().size()),
			};

			m_DrawCommands[key] = cmd;
		}
	}

	auto SceneRenderer::Resize(uint32_t width, uint32_t height) -> void
	{
		EP_PROFILE_FN("SceneRenderer::Resize")

		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		m_GeometryPipeline->Resize(m_Width, m_Height);
	}

	auto SceneRenderer::GeometryPass() -> void
	{
		EP_PROFILE_FN("SceneRenderer::GeometryPass")

		const auto& dm = DeviceManager::Get();
		auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		m_CommandList->open();
		m_CommandList->beginTimerQuery(m_TimerQueries.at(frameIndex));
		m_CommandList->beginMarker("Geometry Pass");

		// Clear framebuffer if needed
		const auto& framebuffer = m_GeometryPipeline->GetSpecification().Framebuffer;

		if (framebuffer->GetSpecification().ClearColorOnLoad)
		{
			const auto& clearColor = framebuffer->GetSpecification().ClearColor;
			for (uint32_t i = 0; i < framebuffer->GetFramebuffer()->getDesc().colorAttachments.size(); i++)
				nvrhi::utils::ClearColorAttachment(m_CommandList, framebuffer->GetFramebuffer(), i, nvrhi::Color(clearColor.r, clearColor.g, clearColor.b, clearColor.a));
		}

		if (framebuffer->GetSpecification().ClearDepthOnLoad)
		{
			const auto& spec = framebuffer->GetSpecification();
			nvrhi::utils::ClearDepthStencilAttachment(m_CommandList, framebuffer->GetFramebuffer(), spec.DepthClearValue, spec.StencilClearValue);
		}

		// Setup graphics state
		nvrhi::GraphicsState state{
			.pipeline = m_GeometryPipeline->GetPipeline(),
			.framebuffer = framebuffer->GetFramebuffer(),
		};

		// Viewport and scissor
		state.viewport.viewports = { nvrhi::Viewport(static_cast<float>(m_Width), static_cast<float>(m_Height)) };
		state.viewport.scissorRects = { nvrhi::Rect(m_Width, m_Height) };

		// Push constants forward decl
		struct PushConstants
		{
			glm::mat4 Transform;
			uint32_t InstanceOffset;
			int32_t DiffuseMapIndex;
			int32_t NormalMapIndex;
			int32_t RoughMetMapIndex;
			float Metallic;
			float Roughness;
		} pushConstants{};

		// Binding sets
		const auto& bindingLayouts = m_GeometryPipeline->GetSpecification().Shader->GetBindingLayouts();

		// Set 0
		nvrhi::BindingSetDesc desc{};
		desc.bindings = {
			nvrhi::BindingSetItem::PushConstants(0, sizeof(PushConstants)),
			nvrhi::BindingSetItem::ConstantBuffer(1, m_CameraUB->GetBuffer()),
			nvrhi::BindingSetItem::ConstantBuffer(2, m_LightsUB->GetBuffer()),
			nvrhi::BindingSetItem::Sampler(0, m_Sampler),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_InstanceTransformsSB->GetBuffer())
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);
		
		// Set 1
		const auto& descriptorTable = m_GeometryPipeline->GetSpecification().Shader->GetDescriptorTable();
		state.addBindingSet(descriptorTable);

		for (const auto& [key, drawCmd] : m_DrawCommands)
		{
			const uint32_t instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
			if (instanceCount == 0)
				continue;

			for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
			{
				nvrhi::VertexBufferBinding vtxBufBinding{
					.buffer = submesh.VertexBuffer->GetBuffer(),
					.slot = 0,
					.offset = 0,
				};

				state.vertexBuffers.resize(1);
				state.vertexBuffers[0] = vtxBufBinding;
				state.indexBuffer.buffer = submesh.IndexBuffer->GetBuffer();
				state.indexBuffer.format = nvrhi::Format::R32_UINT;
				state.indexBuffer.offset = 0;
				m_CommandList->setGraphicsState(state);

				pushConstants.Transform = submesh.LocalTransform;
				pushConstants.InstanceOffset = drawCmd.InstanceOffset;

				for (const auto& p : submesh.Primitives)
				{
					pushConstants.DiffuseMapIndex = p.Material->DiffuseMapIndex;
					pushConstants.NormalMapIndex = p.Material->NormalMapIndex;
					pushConstants.RoughMetMapIndex = p.Material->RoughMetMapIndex;
					pushConstants.Metallic = p.Material->Metallic;
					pushConstants.Roughness = p.Material->Roughness;
					m_CommandList->setPushConstants(&pushConstants, sizeof(PushConstants));

					nvrhi::DrawArguments drawArgs{
						.vertexCount = static_cast<uint32_t>(p.IndexCount),
						.instanceCount = instanceCount,
						.startIndexLocation = p.FirstIndex,
						.startVertexLocation = p.FirstVertex,
					};

					m_CommandList->drawIndexed(drawArgs);

					m_DrawStatistics.DrawCalls++;
				}
				m_DrawStatistics.Submeshes++;
			}
			m_DrawStatistics.Instances += instanceCount;
			m_DrawStatistics.Meshes++;
		}

		m_CommandList->endMarker();
		m_CommandList->endTimerQuery(m_TimerQueries.at(frameIndex));
		m_CommandList->close();
		device->executeCommandList(m_CommandList);
	}
}