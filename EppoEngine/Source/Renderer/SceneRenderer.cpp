#include "pch.h"
#include "Renderer/SceneRenderer.h"

#include "Core/Application.h"
#include "Project/Project.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nvrhi/utils.h>

#include <ranges>

namespace Eppo
{
	SceneRenderer::SceneRenderer(const Ref<Scene>& scene, const SceneRendererSpecification& specification)
		: m_Scene(scene), m_DebugRenderingEnabled(specification.EnableDebugRendering)
	{
		EP_PROFILE_FN("SceneRenderer::SceneRenderer")

		const auto& dm = DeviceManager::Get();
		auto device = dm->GetDevice();
		const auto& renderer = dm->GetRenderer();
		m_CommandList = device->createCommandList();

		m_Width = specification.Width == 0 ? Application::Get().GetWindow()->GetWidth() : specification.Width;
		m_Height = specification.Height == 0 ? Application::Get().GetWindow()->GetHeight() : specification.Height;

		m_Sampler = CreateRef<Sampler>();

		// Create render passes
		// Geometry
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

			auto pipeline = CreateRef<Pipeline>(pipelineSpec);

			RenderPassSpecification renderPassSpec{
				.Name = "Geometry",
				.Pipeline = pipeline,
				.ClearColor = true,
				.ClearDepth = true,
			};

			m_GeometryPass = RenderPass(renderPassSpec);
		}

		// Skybox
		{
			PipelineSpecification pipelineSpec{
				.Shader = renderer->GetShader("skybox"),
				.Framebuffer = m_GeometryPass.GetPipeline()->GetSpecification().Framebuffer,
				.Width = m_Width,
				.Height = m_Height,
				.CullMode = nvrhi::RasterCullMode::None,
				.DepthTestEnable = true,
				.DepthWriteEnable = false,
				.DepthFunc = nvrhi::ComparisonFunc::LessOrEqual,
			};

			auto pipeline = CreateRef<Pipeline>(pipelineSpec);

			RenderPassSpecification renderPassSpec{
				.Name = "Skybox",
				.Pipeline = pipeline,
			};

			m_SkyPass = RenderPass(renderPassSpec);
		}

		// Wireframes
		{
			PipelineSpecification pipelineSpec{
				.Shader = renderer->GetShader("wireframe"),
				.Framebuffer = m_GeometryPass.GetPipeline()->GetSpecification().Framebuffer,
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

			auto pipeline = CreateRef<Pipeline>(pipelineSpec);

			RenderPassSpecification renderPassSpec{
				.Name = "Wireframes",
				.Pipeline = pipeline,
			};

			m_WireframePass = RenderPass(renderPassSpec);
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
		renderPass(m_SkyPass);
	    renderPass(m_WireframePass);

		PassStatistics sceneStats;
		sceneStats += m_GeometryPass.GetStats();
		sceneStats += m_SkyPass.GetStats();
	    sceneStats += m_WireframePass.GetStats();
		const float sceneTime = m_GeometryPass.GetTimeMs(frameIndex) + m_SkyPass.GetTimeMs(frameIndex) + m_WireframePass.GetTimeMs(frameIndex);
		ImGui::Text("Scene total: %u draw calls, %.2fms", sceneStats.DrawCalls, sceneTime);

		// UI is tracked and reported separately from the scene.
		ImGui::SeparatorText("UI");
		const PassStatistics uiStats = imguiRenderer->GetStats();
		ImGui::Text("UI: %.2fms", imguiRenderer->GetGPUTime(frameIndex));
		ImGui::Text("Draw calls: %u", uiStats.DrawCalls);
		ImGui::Text("Vertices: %u", uiStats.Vertices);
		ImGui::Text("Indices: %u", uiStats.Indices);

		// Everything on screen: scene passes plus UI.
		const std::string totalLabel = std::format("Total: {:.2f}ms", sceneTime + imguiRenderer->GetGPUTime(frameIndex));
		ImGui::SeparatorText(totalLabel.c_str());
		ImGui::Text("Draw calls: %u", sceneStats.DrawCalls + uiStats.DrawCalls);
		ImGui::Text("Vertices: %u", sceneStats.Vertices + uiStats.Vertices);
		ImGui::Text("Indices: %u", sceneStats.Indices + uiStats.Indices);

		ImGui::End();
	}

	auto SceneRenderer::BeginScene(const EditorCamera& camera) -> void
	{
        EP_PROFILE_FN("SceneRenderer::BeginScene")

		m_CameraData.View = camera.GetViewMatrix();
        m_CameraData.Projection = camera.GetProjectionMatrix();
        m_CameraData.ViewProjection = camera.GetViewProjection();
        m_CameraData.Position = glm::vec4(camera.GetPosition(), 0.0f);

		BeginSceneInternal();
	}

	auto SceneRenderer::BeginScene(const SceneCamera& camera, const glm::mat4& transform) -> void
	{
		EP_PROFILE_FN("SceneRenderer::BeginScene")

		m_CameraData.View = glm::inverse(transform);
        m_CameraData.Projection = camera.GetProjectionMatrix();
        m_CameraData.ViewProjection = m_CameraData.Projection * m_CameraData.View;
        m_CameraData.Position = glm::vec4(glm::vec3(transform[3]), 0.0f);

		BeginSceneInternal();
	}

	auto SceneRenderer::BeginSceneInternal() -> void
	{
        m_DrawCommands.clear();
        m_LightData.NumLights = 0;

		m_CameraData.InverseViewProjection = glm::inverse(m_CameraData.ViewProjection);

        m_CameraUB->SetData(&m_CameraData, sizeof(CameraData));
        m_LightsUB->SetData(&m_LightData, sizeof(LightData));
        m_EnvironmentUB->SetData(&m_EnvironmentData, sizeof(EnvironmentData));
	}

	auto SceneRenderer::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, const float intensity) -> void
	{
		if (m_LightData.NumLights >= MaxPointLights)
		{
			Log::Warn("Scene has more than {} point lights; extra lights are ignored.", MaxPointLights);
			return;
		}

		auto& light = m_LightData.Lights.at(m_LightData.NumLights);
		light.Position = glm::vec4(position, 1.0f);
		light.Color = glm::vec4(color, intensity);

	    m_LightData.NumLights++;
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

		const auto& descriptorTable = m_GeometryPass.GetPipeline()->GetSpecification().Shader->GetDescriptorTable();

		// Resize descriptor table
		uint32_t imageCount = 0;
		for (const auto& drawCmd : m_DrawCommands | std::views::values)
			imageCount += drawCmd.ImageCount;

		device->resizeDescriptorTable(descriptorTable, imageCount, false);

		// Write descriptor table and gather instance transforms.
		uint32_t imageOffset = 0;
		std::vector<glm::mat4> instanceTransforms;
		for (auto& drawCmd : m_DrawCommands | std::views::values)
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
		SkyPass();
		WireframePass();
	}

	auto SceneRenderer::GetFinalImage() const -> const Ref<Image>&
	{
		return m_GeometryPass.GetPipeline()->GetSpecification().Framebuffer->GetFinalImage();
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

		m_GeometryPass.Resize(m_Width, m_Height);
	}

	auto SceneRenderer::GeometryPass() -> void
	{
		EP_PROFILE_FN("SceneRenderer::GeometryPass")

		const auto device = DeviceManager::Get()->GetDevice();

		nvrhi::GraphicsState state = m_GeometryPass.Begin(m_CommandList);
		PassStatistics& stats = m_GeometryPass.GetStats();

		// Push constants
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
		const auto& pipeline = m_GeometryPass.GetPipeline();
		const auto& bindingLayouts = pipeline->GetSpecification().Shader->GetBindingLayouts();

		// Set 0
		nvrhi::BindingSetDesc desc{};
		desc.bindings = {
			nvrhi::BindingSetItem::PushConstants(0, sizeof(PushConstants)),
			nvrhi::BindingSetItem::ConstantBuffer(1, m_CameraUB->GetBuffer()),
			nvrhi::BindingSetItem::ConstantBuffer(2, m_LightsUB->GetBuffer()),
			nvrhi::BindingSetItem::ConstantBuffer(3, m_EnvironmentUB->GetBuffer()),
			nvrhi::BindingSetItem::Sampler(0, m_Sampler->GetSampler()),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_InstanceTransformsSB->GetBuffer())
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);

		// Set 1
		const auto& descriptorTable = pipeline->GetSpecification().Shader->GetDescriptorTable();
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

		m_GeometryPass.End(m_CommandList);
		m_GeometryPass.Submit(m_CommandList);
	}

	auto SceneRenderer::SkyPass() -> void
	{
		EP_PROFILE_FN("SceneRenderer::SkyPass")

		const auto device = DeviceManager::Get()->GetDevice();

		// Draw into the geometry framebuffer without clearing: the fullscreen
		// triangle only survives where geometry left the depth at the far plane,
		// so it fills the background and leaves lit meshes untouched.
		nvrhi::GraphicsState state = m_SkyPass.Begin(m_CommandList);

		const auto& pipeline = m_SkyPass.GetPipeline();
		const auto& bindingLayouts = pipeline->GetSpecification().Shader->GetBindingLayouts();

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

		m_SkyPass.End(m_CommandList);
		m_SkyPass.Submit(m_CommandList);
	}

	auto SceneRenderer::WireframePass() -> void
	{
		EP_PROFILE_FN("SceneRenderer::WireframePass")

		// Debug overlays are opt-in; bail before touching any GPU state.
		if (!m_DebugRenderingEnabled)
			return;

		constexpr auto colliderColor = glm::vec4(0.2f, 0.8f, 0.3f, 1.0f);
		constexpr auto highlightColor = glm::vec4(0.91f, 0.39f, 0.11f, 1.0f); // Eppo orange
		constexpr auto meshWireframeColor = glm::vec4(0.45f, 0.63f, 0.95f, 1.0f); // Soft blue

		// Gather this frame's wireframe draws. Each is a mesh + world transform +
		// color; the unit collider primitives carry the collider's size in the
		// transform scale. Wireframes are sourced from the scene, not the geometry
		// draw list: that list is batched by mesh asset (no entity to recover) and
		// misses collider-only entities entirely. An entity may carry more than one
		// collider, so each type is checked independently (matching GatherColliders).
		struct WireframeDraw
		{
			Ref<Mesh> Mesh = nullptr;
			glm::mat4 Transform = glm::mat4(1.0f);
			glm::vec4 Color = glm::vec4(1.0f);
		};
		std::vector<WireframeDraw> wireframes;

		const auto& assetManager = Project::GetActive()->GetAssetManager();

		if (m_ShowColliders)
		{
			// Unit collider primitives from the AssetManager (lazy-cached there).
			// Dimensions match the scale math below: cube half-extent 1, sphere
			// radius 1, capsule radius 1 / total height 2 (so Height maps to Height/2).
			const auto boxMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cube));
			const auto sphereMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Sphere));
			const auto capsuleMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Capsule));

			m_Scene->ForEachEntity([&](Entity entity)
			{
				const glm::mat4 world = m_Scene->GetWorldTransform(entity);

				if (entity.HasComponent<BoxColliderComponent>())
				{
					const auto& c = entity.GetComponent<BoxColliderComponent>();
					wireframes.push_back({ boxMesh, glm::scale(glm::translate(world, c.Offset), c.HalfSize), colliderColor });
				}

				if (entity.HasComponent<SphereColliderComponent>())
				{
					const auto& c = entity.GetComponent<SphereColliderComponent>();
					wireframes.push_back({ sphereMesh, glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius)), colliderColor });
				}

				if (entity.HasComponent<CapsuleColliderComponent>())
				{
					const auto& c = entity.GetComponent<CapsuleColliderComponent>();
					// Unit capsule: radius 1, hemisphere centers at +-1, so height maps to Height/2.
					wireframes.push_back({ capsuleMesh, glm::scale(glm::translate(world, c.Offset), glm::vec3(c.Radius, c.Height / 2.0f, c.Radius)), colliderColor });
				}
			});
		}

		// Mesh wireframe overlay: every entity with a MeshComponent gets its full
		// mesh drawn as a wireframe outline. Gives a scene-wide wireframe debug view
		// independent of colliders or selection.
		if (m_ShowWireframes)
		{
			m_Scene->ForEachEntity([&](Entity entity)
			{
				if (entity.HasComponent<MeshComponent>())
				{
					if (const auto& mc = entity.GetComponent<MeshComponent>(); mc.MeshHandle)
					{
						const auto mesh = assetManager->GetOrLoadAsset<Mesh>(mc.MeshHandle);
						const glm::mat4 world = m_Scene->GetWorldTransform(entity);
						wireframes.push_back({ mesh, world, meshWireframeColor });
					}
				}
			});
		}

		// Selection highlight: a wireframe box around the entity's mesh AABB.
		// Uses the unit cube (vertices at +-1) scaled to the mesh's local-space
		// half-extent and centered at the AABB center, then transformed to world.
		if (m_HighlightedEntity && m_HighlightedEntity.HasComponent<MeshComponent>())
		{
			if (const auto& mc = m_HighlightedEntity.GetComponent<MeshComponent>(); mc.MeshHandle)
			{
				const auto mesh = assetManager->GetOrLoadAsset<Mesh>(mc.MeshHandle);
				if (const auto& bounds = mesh->GetBounds(); bounds.IsValid())
				{
					const auto boxMesh = assetManager->GetOrLoadAsset<Mesh>(static_cast<uint64_t>(MeshPrimitiveType::Cube));
					const glm::mat4 world = m_Scene->GetWorldTransform(m_HighlightedEntity);
					const glm::mat4 boxTransform = glm::scale(glm::translate(world, bounds.GetCenter()), bounds.GetHalfExtent());
					wireframes.push_back({ boxMesh, boxTransform, highlightColor });
				}
			}
		}

		if (wireframes.empty())
			return;

		const auto device = DeviceManager::Get()->GetDevice();

		// One instance transform per draw; the shader indexes it by InstanceOffset.
		std::vector<glm::mat4> instanceTransforms;
		instanceTransforms.reserve(wireframes.size());
		for (const auto& draw : wireframes)
			instanceTransforms.push_back(draw.Transform);

		const uint64_t requiredSize = instanceTransforms.size() * sizeof(glm::mat4);
		if (!m_WireframeInstanceSB)
			m_WireframeInstanceSB = CreateRef<StorageBuffer>(sizeof(glm::mat4), requiredSize, "StorageBuffer Wireframe Instance Transforms");
		m_WireframeInstanceSB->SetData(instanceTransforms.data(), requiredSize);

		nvrhi::GraphicsState state = m_WireframePass.Begin(m_CommandList);
		PassStatistics& stats = m_WireframePass.GetStats();

		// Matches wireframe.vert: { Transform, WireframeColor, InstanceOffset }.
		struct PushConstants
		{
			glm::mat4 Transform;
			glm::vec4 Color;
			uint32_t InstanceOffset;
		} pushConstants{};

		const auto& pipeline = m_WireframePass.GetPipeline();
		const auto& bindingLayouts = pipeline->GetSpecification().Shader->GetBindingLayouts();

		nvrhi::BindingSetDesc desc{};
		desc.bindings = {
			nvrhi::BindingSetItem::PushConstants(0, sizeof(PushConstants)),
			nvrhi::BindingSetItem::ConstantBuffer(1, m_CameraUB->GetBuffer()),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_WireframeInstanceSB->GetBuffer()),
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);

		for (uint32_t drawIndex = 0; const auto& draw : wireframes)
		{
			for (const auto& submesh : draw.Mesh->GetSubmeshes())
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
				pushConstants.Color = draw.Color;
				pushConstants.InstanceOffset = drawIndex;

				for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : submesh.Primitives)
				{
					m_CommandList->setPushConstants(&pushConstants, sizeof(PushConstants));

					nvrhi::DrawArguments drawArgs{
						.vertexCount = static_cast<uint32_t>(indexCount),
						.instanceCount = 1,
						.startIndexLocation = firstIndex,
						.startVertexLocation = firstVertex,
					};

					m_CommandList->drawIndexed(drawArgs);

					stats.DrawCalls++;
					stats.Vertices += static_cast<uint32_t>(vertexCount);
					stats.Indices += static_cast<uint32_t>(indexCount);
				}
				stats.Submeshes++;
			}
			stats.Meshes++;
			stats.Instances++;
			++drawIndex;
		}

		m_WireframePass.End(m_CommandList);
		m_WireframePass.Submit(m_CommandList);
	}
}