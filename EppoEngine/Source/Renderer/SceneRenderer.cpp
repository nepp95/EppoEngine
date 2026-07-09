#include "pch.h"
#include "Renderer/SceneRenderer.h"

#include "Core/Application.h"
#include "Project/Project.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"

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

		// Wireframe Pipeline
		// Shares the geometry framebuffer. Draws only selected entities' meshes as
		// wireframe overlays with a flat color. Depth test on / write off so edges
		// appear on visible surfaces only, with no z-fighting side effects.
		{
			PipelineSpecification pipelineSpec{
				.Shader = renderer->GetShader("wireframe"),
				.Framebuffer = m_GeometryPipeline->GetSpecification().Framebuffer,
				.Width = m_Width,
				.Height = m_Height,
				.CullMode = nvrhi::RasterCullMode::None,
				.FillMode = nvrhi::RasterFillMode::Wireframe,
				.DepthTestEnable = true,
				.DepthWriteEnable = false,
				.DepthFunc = nvrhi::ComparisonFunc::LessOrEqual,
				.DepthBias = -1,
				.SlopeScaledDepthBias = -1.0f,
			};

			m_WireframePipeline = CreateRef<Pipeline>(pipelineSpec);
		}

		// Sky Pipeline
		// Renders a fullscreen triangle into the geometry framebuffer AFTER the
		// meshes, filling only background pixels (depth == far). Shares the geometry
		// framebuffer so it resizes with it and composites in the same pass target.
		{
			PipelineSpecification pipelineSpec{
				.Shader = renderer->GetShader("skybox"),
				.Framebuffer = m_GeometryPipeline->GetSpecification().Framebuffer,
				.Width = m_Width,
				.Height = m_Height,
				.CullMode = nvrhi::RasterCullMode::None,
				.DepthTestEnable = true,
				.DepthWriteEnable = false,
				.DepthFunc = nvrhi::ComparisonFunc::LessOrEqual,
			};

			m_SkyPipeline = CreateRef<Pipeline>(pipelineSpec);
		}

		// Uniform buffers
		m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Camera");
		m_LightsUB = CreateRef<UniformBuffer>(sizeof(LightData), "UniformBuffer Lights");
		m_EnvironmentUB = CreateRef<UniformBuffer>(sizeof(EnvironmentData), "UniformBuffer Environment");
	}

	auto SceneRenderer::RenderGui() const -> void
	{
		EP_PROFILE_FN("SceneRenderer::RenderGui")

		const auto& app = Application::Get();
		const auto& dm = DeviceManager::Get();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		const auto& imguiRenderer = app.GetImGuiLayer()->GetMainImGuiRenderer();

		// One collapsible row per scene pass: its GPU time plus draw-call breakdown.
		const auto renderPass = [frameIndex](const RenderPass& pass)
		{
			const PassStatistics& stats = pass.GetStats();
			if (!ImGui::TreeNodeEx(pass.GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s: %.2fms", pass.GetName().c_str(), pass.GetTimeMs(frameIndex)))
				return;

			ImGui::Text("Draw calls: %u", stats.DrawCalls);
			ImGui::Text("Meshes: %u", stats.Meshes);
			ImGui::Text("Submeshes: %u", stats.Submeshes);
			ImGui::Text("Instances: %u", stats.Instances);
			ImGui::Text("Vertices: %u", stats.Vertices);
			ImGui::Text("Indices: %u", stats.Indices);
			ImGui::TreePop();
		};

		ImGui::Begin("Scene Renderer");

		// Scene passes and their subtotal.
		ImGui::SeparatorText("Scene");
		renderPass(m_GeometryPass);
		renderPass(m_WireframePass);
		renderPass(m_SkyPass);

		PassStatistics sceneStats;
		sceneStats += m_GeometryPass.GetStats();
		sceneStats += m_WireframePass.GetStats();
		sceneStats += m_SkyPass.GetStats();
		const float sceneTime = m_GeometryPass.GetTimeMs(frameIndex) + m_WireframePass.GetTimeMs(frameIndex) + m_SkyPass.GetTimeMs(frameIndex);
		ImGui::Text("Scene total: %u draw calls, %.2fms", sceneStats.DrawCalls, sceneTime);

		// UI is tracked and reported separately from the scene.
		ImGui::SeparatorText("UI");
		const PassStatistics uiStats = imguiRenderer->GetStats();
		ImGui::Text("UI: %.2fms", imguiRenderer->GetGPUTime(frameIndex));
		ImGui::Text("Draw calls: %u", uiStats.DrawCalls);
		ImGui::Text("Vertices: %u", uiStats.Vertices);
		ImGui::Text("Indices: %u", uiStats.Indices);

		// Everything on screen: scene passes plus UI. Note the UI stats lag by a
		// frame — this panel is part of the UI draw data being built now, so the UI
		// counts reflect the previous frame's Render, whereas the scene counts are
		// this frame's. Close enough for an at-a-glance readout, not a coherent snapshot.
		// SeparatorText takes only a label, so format the total time into it first.
		const std::string totalLabel = std::format("Total: {:.2f}ms", sceneTime + imguiRenderer->GetGPUTime(frameIndex));
		ImGui::SeparatorText(totalLabel.c_str());
		ImGui::Text("Draw calls: %u", sceneStats.DrawCalls + uiStats.DrawCalls);
		ImGui::Text("Vertices: %u", sceneStats.Vertices + uiStats.Vertices);
		ImGui::Text("Indices: %u", sceneStats.Indices + uiStats.Indices);

		ImGui::End();
	}

	auto SceneRenderer::BeginScene(const ScopedPtr<EditorCamera>& camera) -> void
	{
		BeginScene(camera->GetViewMatrix(), camera->GetProjectionMatrix(), camera->GetPosition());
	}

	auto SceneRenderer::BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position) -> void
	{
		EP_PROFILE_FN("SceneRenderer::BeginScene")

		// Reset (per-pass draw stats are reset in each RenderPass::Begin)
		m_DrawCommands.clear();
		m_WireframeDrawCommands.clear();
		m_LightData.NumLights = 0;

		// Set uniforms
		m_CameraData.View = view;
		m_CameraData.Projection = projection;
		m_CameraData.ViewProjection = projection * view;
		m_CameraData.Position = glm::vec4(position, 0.0f);
		m_CameraData.InverseViewProjection = glm::inverse(m_CameraData.ViewProjection);
		m_CameraUB->SetData(&m_CameraData, sizeof(CameraData));
	}

	auto SceneRenderer::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, const float intensity) -> void
	{
		if (m_LightData.NumLights >= MaxPointLights)
		{
			// Warn once per overflow rather than every submit, but never drop silently.
			if (!m_LightOverflowWarned)
			{
				Log::Warn("Scene has more than {} point lights; extra lights are ignored.", MaxPointLights);
				m_LightOverflowWarned = true;
			}
			return;
		}

		auto& light = m_LightData.Lights[m_LightData.NumLights++];
		light.Position = glm::vec4(position, 1.0f);
		light.Color = glm::vec4(color, intensity);
	}

	auto SceneRenderer::SubmitEnvironment(const EnvironmentSettings& environment) -> void
	{
		m_EnvironmentData.ZenithColor = glm::vec4(environment.ZenithColor, 1.0f);
		m_EnvironmentData.HorizonColor = glm::vec4(environment.HorizonColor, 1.0f);
		m_EnvironmentData.GroundColor = glm::vec4(environment.GroundColor, 1.0f);
		// Params.y is the skybox flag; kept 0 until an HDR sky loader lands.
		m_EnvironmentData.Params = glm::vec4(environment.AmbientIntensity, 0.0f, 0.0f, 0.0f);
	}

	auto SceneRenderer::EndScene() -> void
	{
		EP_PROFILE_FN("SceneRenderer::EndScene")

		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		// --- Geometry pass descriptor table ---
		const auto& descriptorTable = m_GeometryPipeline->GetSpecification().Shader->GetDescriptorTable();

		// Resize descriptor table
		uint32_t imageCount = 0;
		for (const auto& [key, drawCmd] : m_DrawCommands)
			imageCount += drawCmd.ImageCount;

		device->resizeDescriptorTable(descriptorTable, imageCount, false);

		// Write descriptor table and gather instance transforms for both passes.
		// Wireframe meshes reuse the same instance buffer after the geometry ones
		// so that WireframePass also reads them from a single SRV.
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

		// Wireframe draw commands — no texture info, just transforms
		for (auto& [key, drawCmd] : m_WireframeDrawCommands)
		{
			drawCmd.InstanceOffset = static_cast<uint32_t>(instanceTransforms.size());
			instanceTransforms.insert(instanceTransforms.end(), drawCmd.Transforms.begin(), drawCmd.Transforms.end());
		}

		// Reallocate instance transforms buffer if needed
		const uint64_t requiredSize = instanceTransforms.size() * sizeof(glm::mat4);

		if (!m_InstanceTransformsSB)
			m_InstanceTransformsSB = CreateRef<StorageBuffer>(static_cast<uint32_t>(sizeof(glm::mat4)), requiredSize, "StorageBuffer Instance Transforms");

		m_InstanceTransformsSB->SetData(instanceTransforms.data(), requiredSize);

		// Upload per-frame lighting/environment state gathered via Submit*.
		m_LightsUB->SetData(&m_LightData, sizeof(LightData));
		m_EnvironmentUB->SetData(&m_EnvironmentData, sizeof(EnvironmentData));

		GeometryPass();
		WireframePass();
		SkyPass();

		// All pass command lists have been executed; read their GPU timers back.
		m_GeometryPass.Readback(frameIndex);
		m_WireframePass.Readback(frameIndex);
		m_SkyPass.Readback(frameIndex);
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

	auto SceneRenderer::SubmitWireframeMesh(const AssetHandle meshHandle, const glm::mat4& transform) -> void
	{
		const DrawKey key{
			.ID = meshHandle,
		};

		if (m_WireframeDrawCommands.contains(key))
		{
			auto& drawCmd = m_WireframeDrawCommands.at(key);
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

			m_WireframeDrawCommands[key] = cmd;
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
		m_GeometryPass.Begin(m_CommandList, frameIndex, "Geometry Pass");
		PassStatistics& stats = m_GeometryPass.GetStats();

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
		state.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(m_Width), static_cast<int>(m_Height)) };

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
			nvrhi::BindingSetItem::ConstantBuffer(3, m_EnvironmentUB->GetBuffer()),
			nvrhi::BindingSetItem::Sampler(0, m_Sampler),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_InstanceTransformsSB->GetBuffer())
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);
		
		// Set 1
		const auto& descriptorTable = m_GeometryPipeline->GetSpecification().Shader->GetDescriptorTable();
		state.addBindingSet(descriptorTable);

		for (const auto& drawCmd : m_DrawCommands | std::views::values)
		{
			const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
			if (instanceCount == 0)
				continue;

			for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
			{
                const nvrhi::VertexBufferBinding vtxBufBinding{
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

				for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
				{
					pushConstants.DiffuseMapIndex = material->DiffuseMapIndex;
					pushConstants.NormalMapIndex = material->NormalMapIndex;
					pushConstants.RoughMetMapIndex = material->RoughMetMapIndex;
					pushConstants.Metallic = material->Metallic;
					pushConstants.Roughness = material->Roughness;
					m_CommandList->setPushConstants(&pushConstants, sizeof(PushConstants));

					nvrhi::DrawArguments drawArgs{
						.vertexCount = static_cast<uint32_t>(indexCount),
						.instanceCount = instanceCount,
						.startIndexLocation = firstIndex,
						.startVertexLocation = firstVertex,
					};

					m_CommandList->drawIndexed(drawArgs);

					stats.DrawCalls++;
					stats.Vertices += static_cast<uint32_t>(vertexCount) * instanceCount;
					stats.Indices += static_cast<uint32_t>(indexCount) * instanceCount;
				}
				stats.Submeshes++;
			}
			stats.Instances += instanceCount;
			stats.Meshes++;
		}

		m_GeometryPass.End(m_CommandList, frameIndex);
		m_CommandList->close();
		device->executeCommandList(m_CommandList);
	}

	auto SceneRenderer::WireframePass() -> void
	{
		EP_PROFILE_FN("SceneRenderer::WireframePass")

		if (m_WireframeDrawCommands.empty())
			return;

		const auto& dm = DeviceManager::Get();
		auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		m_CommandList->open();
		m_WireframePass.Begin(m_CommandList, frameIndex, "Wireframe Pass");
		PassStatistics& stats = m_WireframePass.GetStats();

		const auto& framebuffer = m_GeometryPipeline->GetSpecification().Framebuffer;

		nvrhi::GraphicsState state{
			.pipeline = m_WireframePipeline->GetPipeline(),
			.framebuffer = framebuffer->GetFramebuffer(),
		};
		state.viewport.viewports = { nvrhi::Viewport(static_cast<float>(m_Width), static_cast<float>(m_Height)) };
		state.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(m_Width), static_cast<int>(m_Height)) };

		struct WireframePushConstants
		{
			glm::mat4 Transform;         // offset 0,  64 bytes
			uint32_t InstanceOffset;     // offset 64,  4 bytes
			float Padding[3];            // offset 68, 12 bytes (matches std140 float4 alignment)
			glm::vec4 Color;             // offset 80, 16 bytes
		} pushConstants{};

		pushConstants.Color = m_WireframeColor;

		const auto& bindingLayouts = m_WireframePipeline->GetSpecification().Shader->GetBindingLayouts();

		nvrhi::BindingSetDesc desc{};
		desc.bindings = {
			nvrhi::BindingSetItem::PushConstants(0, sizeof(WireframePushConstants)),
			nvrhi::BindingSetItem::ConstantBuffer(1, m_CameraUB->GetBuffer()),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_InstanceTransformsSB->GetBuffer()),
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);

		for (const auto& drawCmd : m_WireframeDrawCommands | std::views::values)
		{
			const auto instanceCount = static_cast<uint32_t>(drawCmd.Transforms.size());
			if (instanceCount == 0)
				continue;

			for (const auto& submesh : drawCmd.Mesh->GetSubmeshes())
			{
				const nvrhi::VertexBufferBinding vtxBufBinding{
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

				for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
				{
					m_CommandList->setPushConstants(&pushConstants, sizeof(WireframePushConstants));

					nvrhi::DrawArguments drawArgs{
						.vertexCount = static_cast<uint32_t>(indexCount),
						.instanceCount = instanceCount,
						.startIndexLocation = firstIndex,
						.startVertexLocation = firstVertex,
					};

					m_CommandList->drawIndexed(drawArgs);

					stats.DrawCalls++;
					stats.Vertices += static_cast<uint32_t>(vertexCount) * instanceCount;
					stats.Indices += static_cast<uint32_t>(indexCount) * instanceCount;
				}
				stats.Submeshes++;
			}
			stats.Instances += instanceCount;
			stats.Meshes++;
		}

		m_WireframePass.End(m_CommandList, frameIndex);
		m_CommandList->close();
		device->executeCommandList(m_CommandList);
	}

	auto SceneRenderer::SkyPass() -> void
	{
		EP_PROFILE_FN("SceneRenderer::SkyPass")

		const auto& dm = DeviceManager::Get();
        const auto device = dm->GetDevice();
		const uint32_t frameIndex = dm->GetCurrentBackBufferIndex();
		EP_ASSERT(frameIndex < dm->GetParams().MaxFramesInFlight);

		m_CommandList->open();
		m_SkyPass.Begin(m_CommandList, frameIndex, "Sky Pass");

		// Draw into the geometry framebuffer without clearing: the fullscreen
		// triangle only survives where geometry left the depth at the far plane,
		// so it fills the background and leaves lit meshes untouched.
		const auto& framebuffer = m_GeometryPipeline->GetSpecification().Framebuffer;

		nvrhi::GraphicsState state{
			.pipeline = m_SkyPipeline->GetPipeline(),
			.framebuffer = framebuffer->GetFramebuffer(),
		};
		state.viewport.viewports = { nvrhi::Viewport(static_cast<float>(m_Width), static_cast<float>(m_Height)) };
		state.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(m_Width), static_cast<int>(m_Height)) };

		const auto& bindingLayouts = m_SkyPipeline->GetSpecification().Shader->GetBindingLayouts();

		nvrhi::BindingSetDesc desc{};
		desc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(1, m_CameraUB->GetBuffer()),
			nvrhi::BindingSetItem::ConstantBuffer(3, m_EnvironmentUB->GetBuffer()),
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);

		m_CommandList->setGraphicsState(state);

        constexpr nvrhi::DrawArguments drawArgs{
			.vertexCount = 3,
			.instanceCount = 1,
		};
		m_CommandList->draw(drawArgs);

		// One non-indexed fullscreen-triangle draw (3 vertices, no index buffer).
		PassStatistics& stats = m_SkyPass.GetStats();
		stats.DrawCalls++;
		stats.Vertices += drawArgs.vertexCount;

		m_SkyPass.End(m_CommandList, frameIndex);
		m_CommandList->close();
		device->executeCommandList(m_CommandList);
	}
}