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

namespace Eppo
{
	class DebugRenderer
	{
	public:
		DebugRenderer(uint32_t width, uint32_t height, const Ref<Framebuffer>& targetFramebuffer);
		~DebugRenderer() = default;


		// Wireframe of an arbitrary mesh (e.g. the selected entity's mesh).
		auto DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color) -> void;

		auto DrawBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color) -> void;
		auto DrawSphere(const glm::mat4& transform, float radius, const glm::vec4& color) -> void;
		auto DrawCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color) -> void;
		auto DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) -> void;

		// Build once: unit-sized procedural meshes reused for every primitive draw.
		auto CreatePrimitives() -> void;
	};
}
