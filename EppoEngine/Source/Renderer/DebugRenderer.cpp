#include "pch.h"
#include "Renderer/DebugRenderer.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Application.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Vertex.h"
#include "Renderer/VertexBuffer.h"

#include <nvrhi/utils.h>

namespace Eppo
{
	namespace
	{
		// Unit capsule (radius 1, cylinder half-height 1) around +Y, built as a
		// lat-long surface of revolution: bottom hemisphere, a cylindrical band,
		// then the top hemisphere. Normals are radial/axial and already unit length.
		auto BuildCapsuleGeometry() -> DebugRenderer::DebugGeometry
		{
			constexpr uint32_t stacks = 16;
			constexpr uint32_t sectors = 24;
			constexpr float radius = 1.0f;
			constexpr float halfHeight = 1.0f;
			constexpr float cylFrac = 0.35f; // share of the stacks spent on the cylinder
			constexpr float hemiArc = (1.0f - cylFrac) / 2.0f;
			constexpr float pi = 3.14159265358979323846f;

			auto profile = [radius, halfHeight, cylFrac, hemiArc, pi](float v) -> std::pair<float, float>
			{
				if (v < hemiArc)
				{
					const float phi = -pi / 2.0f + (v / hemiArc) * (pi / 2.0f); // -PI/2 .. 0
					return { -halfHeight + radius * std::sin(phi), radius * std::cos(phi) };
				}
				if (v <= 1.0f - hemiArc)
				{
					const float f = (v - hemiArc) / cylFrac;
					return { -halfHeight + f * (2.0f * halfHeight), radius };
				}
				const float phi = ((v - (1.0f - hemiArc)) / hemiArc) * (pi / 2.0f); // 0 .. PI/2
				return { halfHeight + radius * std::sin(phi), radius * std::cos(phi) };
			};

			std::vector<Vertex> vertices;
			vertices.reserve((stacks + 1) * (sectors + 1));
			std::vector<uint32_t> indices;
			indices.reserve(stacks * sectors * 6);

			for (uint32_t i = 0; i <= stacks; i++)
			{
				const float v = static_cast<float>(i) / static_cast<float>(stacks);
				const auto [y, r] = profile(v);
				// Radial normal magnitude equals r (radius == 1); axial part derived
				// from the profile's slope. (r, nY) is already unit length.
				const float nY = std::sqrt(std::max(0.0f, 1.0f - r * r)) * (v < 0.5f ? -1.0f : 1.0f);

				for (uint32_t j = 0; j <= sectors; j++)
				{
					const float sector = static_cast<float>(j) * (2.0f * pi / static_cast<float>(sectors));
					const float cx = std::cos(sector);
					const float sz = std::sin(sector);
					vertices.emplace_back(Vertex{ { r * cx, y, r * sz }, { r * cx, nY, r * sz } });
				}
			}

			for (uint32_t i = 0; i < stacks; i++)
			{
				for (uint32_t j = 0; j < sectors; j++)
				{
					const uint32_t a = i * (sectors + 1) + j;
					const uint32_t b = a + sectors + 1;
					indices.push_back(a);
					indices.push_back(b);
					indices.push_back(a + 1);
					indices.push_back(a + 1);
					indices.push_back(b);
					indices.push_back(b + 1);
				}
			}

			const auto vb = CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
			const auto ib = CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));

			Primitive primitive{
				.FirstVertex = 0,
				.FirstIndex = 0,
				.VertexCount = static_cast<uint64_t>(vertices.size()),
				.IndexCount = static_cast<uint64_t>(indices.size()),
				.Material = CreateRef<Material>(),
			};

			return { vb, ib, { primitive }, glm::mat4(1.0f) };
		}

		auto BuildLineGeometry() -> DebugRenderer::DebugGeometry
		{
			std::vector<Vertex> vertices = {
				Vertex{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
				Vertex{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
			};
			std::vector<uint32_t> indices = { 0, 1 };

			const auto vb = CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
			const auto ib = CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));

			Primitive primitive{
				.FirstVertex = 0,
				.FirstIndex = 0,
				.VertexCount = 2,
				.IndexCount = 2,
				.Material = CreateRef<Material>(),
			};

			return { vb, ib, { primitive }, glm::mat4(1.0f) };
		}
	}

	DebugRenderer::DebugRenderer(const uint32_t width, const uint32_t height, const Ref<Framebuffer>& targetFramebuffer)
		: m_TargetFramebuffer(targetFramebuffer), m_Width(width), m_Height(height)
	{
		EP_PROFILE_FN("DebugRenderer::DebugRenderer")

		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();
		const auto& renderer = dm->GetRenderer();

		m_CommandList = device->createCommandList();

		// Shares the scene's geometry framebuffer; draws wireframe overlays with
		// depth test on / write off so edges sit on visible surfaces, biased
		// slightly toward the camera to avoid z-fighting with the solid mesh.
		PipelineSpecification pipelineSpec{
			.Shader = renderer->GetShader("wireframe"),
			.Framebuffer = m_TargetFramebuffer,
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

		m_Pipeline = CreateRef<Pipeline>(pipelineSpec);
		m_CameraUB = CreateRef<UniformBuffer>(sizeof(CameraData), "UniformBuffer Debug Camera");

		CreatePrimitives();
	}

	auto DebugRenderer::CreatePrimitives() -> void
	{
		EP_PROFILE_FN("DebugRenderer::CreatePrimitives")

		auto toGeometry = [](const Ref<Mesh>& mesh) -> DebugGeometry
		{
			const auto& sm = mesh->GetSubmeshes().front();
			return { sm.VertexBuffer, sm.IndexBuffer, sm.Primitives, sm.LocalTransform };
		};

		m_BoxGeometry = toGeometry(Mesh::CreateMeshPrimitive(MeshPrimitiveType::Cube));
		m_SphereGeometry = toGeometry(Mesh::CreateMeshPrimitive(MeshPrimitiveType::Sphere));
		m_CapsuleGeometry = BuildCapsuleGeometry();
		m_LineGeometry = BuildLineGeometry();
	}

	auto DebugRenderer::SetCamera(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position) -> void
	{
		m_CameraData.View = view;
		m_CameraData.Projection = projection;
		m_CameraData.ViewProjection = projection * view;
		m_CameraData.Position = glm::vec4(position, 0.0f);
		m_CameraData.InverseViewProjection = glm::inverse(m_CameraData.ViewProjection);
		m_CameraUB->SetData(&m_CameraData, sizeof(CameraData));
	}

	auto DebugRenderer::Begin() -> void
	{
		m_Draws.clear();
	}

	auto DebugRenderer::DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color) -> void
	{
		for (const auto& sm : mesh->GetSubmeshes())
			m_Draws.push_back({ { sm.VertexBuffer, sm.IndexBuffer, sm.Primitives, sm.LocalTransform }, transform, color });
	}

	auto DebugRenderer::DrawBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color) -> void
	{
		const glm::mat4 local = glm::scale(glm::mat4(1.0f), halfExtents);
		m_Draws.push_back({ m_BoxGeometry, transform * local, color });
	}

	auto DebugRenderer::DrawSphere(const glm::mat4& transform, const float radius, const glm::vec4& color) -> void
	{
		const glm::mat4 local = glm::scale(glm::mat4(1.0f), glm::vec3(radius));
		m_Draws.push_back({ m_SphereGeometry, transform * local, color });
	}

	auto DebugRenderer::DrawCapsule(const glm::mat4& transform, const float radius, const float height, const glm::vec4& color) -> void
	{
		const glm::mat4 local = glm::scale(glm::mat4(1.0f), glm::vec3(radius, height / 2.0f, radius));
		m_Draws.push_back({ m_CapsuleGeometry, transform * local, color });
	}

	auto DebugRenderer::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) -> void
	{
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), start) * glm::scale(glm::mat4(1.0f), end - start);
		m_Draws.push_back({ m_LineGeometry, transform, color });
	}

	auto DebugRenderer::Render(const uint32_t frameIndex) -> void
	{
		EP_PROFILE_FN("DebugRenderer::Render")

		if (m_Draws.empty())
			return;

		const auto& dm = DeviceManager::Get();
		const auto device = dm->GetDevice();

		// Draw into whatever size the shared framebuffer currently is.
		m_Width = m_TargetFramebuffer->GetWidth();
		m_Height = m_TargetFramebuffer->GetHeight();

		// Gather one instance transform per draw; the shader indexes it by offset.
		std::vector<glm::mat4> instanceTransforms;
		instanceTransforms.reserve(m_Draws.size());
		for (auto& draw : m_Draws)
		{
			draw.InstanceOffset = static_cast<uint32_t>(instanceTransforms.size());
			instanceTransforms.push_back(draw.Transform);
		}

		const uint64_t requiredSize = instanceTransforms.size() * sizeof(glm::mat4);
		if (!m_InstanceTransformsSB)
			m_InstanceTransformsSB = CreateRef<StorageBuffer>(sizeof(glm::mat4), requiredSize, "StorageBuffer Debug Instance Transforms");
		m_InstanceTransformsSB->SetData(instanceTransforms.data(), requiredSize);

		m_CommandList->open();
		m_Pass.Begin(m_CommandList, frameIndex, "Debug Pass");
		PassStatistics& stats = m_Pass.GetStats();

		nvrhi::GraphicsState state{
			.pipeline = m_Pipeline->GetPipeline(),
			.framebuffer = m_TargetFramebuffer->GetFramebuffer(),
		};
		state.viewport.viewports = { nvrhi::Viewport(static_cast<float>(m_Width), static_cast<float>(m_Height)) };
		state.viewport.scissorRects = { nvrhi::Rect(static_cast<int>(m_Width), static_cast<int>(m_Height)) };

		PushConstants pushConstants{};
		const auto& bindingLayouts = m_Pipeline->GetSpecification().Shader->GetBindingLayouts();

		nvrhi::BindingSetDesc desc{};
		desc.bindings = {
			nvrhi::BindingSetItem::PushConstants(0, sizeof(PushConstants)),
			nvrhi::BindingSetItem::ConstantBuffer(1, m_CameraUB->GetBuffer()),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_InstanceTransformsSB->GetBuffer()),
		};

		const auto bindingSet = device->createBindingSet(desc, bindingLayouts.at(0));
		state.addBindingSet(bindingSet);

		for (const auto& draw : m_Draws)
		{
			const nvrhi::VertexBufferBinding vtxBufBinding{
				.buffer = draw.Geometry.VertexBuffer->GetBuffer(),
				.slot = 0,
				.offset = 0,
			};

			state.vertexBuffers.resize(1);
			state.vertexBuffers[0] = vtxBufBinding;
			state.indexBuffer.buffer = draw.Geometry.IndexBuffer->GetBuffer();
			state.indexBuffer.format = nvrhi::Format::R32_UINT;
			state.indexBuffer.offset = 0;
			m_CommandList->setGraphicsState(state);

			pushConstants.Transform = draw.Geometry.LocalTransform;
			pushConstants.InstanceOffset = draw.InstanceOffset;
			pushConstants.Color = draw.Color;

			for (const auto& [firstVertex, firstIndex, vertexCount, indexCount, material] : draw.Geometry.Primitives)
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
			stats.Meshes++;
			stats.Instances++;
		}

		m_Pass.End(m_CommandList, frameIndex);
		m_CommandList->close();
		device->executeCommandList(m_CommandList);

		// Read the GPU timer back once the pass' command list has executed.
		m_Pass.Readback(frameIndex);
	}

	auto DebugRenderer::RenderGui(const uint32_t frameIndex) const -> void
	{
		EP_PROFILE_FN("DebugRenderer::RenderGui")

		const auto& imguiRenderer = Application::Get().GetImGuiLayer()->GetMainImGuiRenderer();

		ImGui::Begin("Debug Renderer");
		const PassStatistics& stats = m_Pass.GetStats();
		if (ImGui::TreeNodeEx(m_Pass.GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s: %.2fms", m_Pass.GetName().c_str(), m_Pass.GetTimeMs(frameIndex)))
		{
			ImGui::Text("Draw calls: %u", stats.DrawCalls);
			ImGui::Text("Meshes: %u", stats.Meshes);
			ImGui::Text("Submeshes: %u", stats.Submeshes);
			ImGui::Text("Instances: %u", stats.Instances);
			ImGui::Text("Vertices: %u", stats.Vertices);
			ImGui::Text("Indices: %u", stats.Indices);
			ImGui::TreePop();
		}
		ImGui::Text("UI: %.2fms", imguiRenderer->GetGPUTime(frameIndex));
		ImGui::End();
	}
}
