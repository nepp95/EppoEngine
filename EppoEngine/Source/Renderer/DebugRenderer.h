#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/RenderPass.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/VertexBuffer.h"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>

#include <vector>

namespace Eppo
{
	// Draws wireframe overlays on top of the scene: selected-entity highlights,
	// collider shapes, debug lines, etc. Owns its own wireframe pipeline (sharing
	// the scene's geometry framebuffer) and a small set of procedural primitives,
	// so all debug drawing funnels through one pass and one stats readout.
	class DebugRenderer
	{
	public:
		DebugRenderer(uint32_t width, uint32_t height, const Ref<Framebuffer>& targetFramebuffer);
		~DebugRenderer() = default;

		auto SetCamera(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position) -> void;

		// Clear the per-frame draw list. Call before submitting this frame's items.
		auto Begin() -> void;

		// Wireframe of an arbitrary mesh (e.g. the selected entity's mesh).
		auto DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color) -> void;

		// Procedural primitives. `transform` is the entity/collider world matrix;
		// the half-extents/radius/height below are baked into the local scale so a
		// single unit primitive serves every size of that shape.
		auto DrawBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color) -> void;
		auto DrawSphere(const glm::mat4& transform, float radius, const glm::vec4& color) -> void;
		auto DrawCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color) -> void;
		auto DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) -> void;

		// Execute the debug pass into the target framebuffer. Call after the scene
		// renderer's EndScene so overlays composite on top of the lit geometry.
		auto Render(uint32_t frameIndex) -> void;
		auto RenderGui(uint32_t frameIndex) const -> void;

		// A single drawable primitive: raw buffers plus the per-submesh metadata
		// the wireframe shader expects (LocalTransform + primitive index ranges).
		struct DebugGeometry
		{
			Ref<VertexBuffer> VertexBuffer = nullptr;
			Ref<IndexBuffer> IndexBuffer = nullptr;
			std::vector<Primitive> Primitives;
			glm::mat4 LocalTransform = glm::mat4(1.0f);
		};

	private:
		struct DebugDraw
		{
			DebugGeometry Geometry;
			glm::mat4 Transform = glm::mat4(1.0f);
			glm::vec4 Color = glm::vec4(1.0f);
			uint32_t InstanceOffset = 0;
		};

		// Mirrors the wireframe shader's Camera cbuffer (see SceneRenderer::CameraData).
		struct CameraData
		{
			glm::mat4 View;
			glm::mat4 Projection;
			glm::mat4 ViewProjection;
			glm::vec4 Position;
			glm::mat4 InverseViewProjection;
		};

		// Matches the wireframe push-constant layout (std140: vec4 Color aligns to 16).
		struct PushConstants
		{
			glm::mat4 Transform;
			uint32_t InstanceOffset;
			float Padding[3];
			glm::vec4 Color;
		};

		// Build once: unit-sized procedural meshes reused for every primitive draw.
		auto CreatePrimitives() -> void;

	private:
		Ref<Framebuffer> m_TargetFramebuffer = nullptr;
		nvrhi::CommandListHandle m_CommandList = nullptr;

		Ref<Pipeline> m_Pipeline = nullptr;
		Ref<UniformBuffer> m_CameraUB = nullptr;
		Ref<StorageBuffer> m_InstanceTransformsSB = nullptr;

		RenderPass m_Pass{ "Debug" };

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		std::vector<DebugDraw> m_Draws;

		DebugGeometry m_BoxGeometry{};
		DebugGeometry m_SphereGeometry{};
		DebugGeometry m_CapsuleGeometry{};
		DebugGeometry m_LineGeometry{};

		CameraData m_CameraData{};
	};
}
