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
	{
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
}
