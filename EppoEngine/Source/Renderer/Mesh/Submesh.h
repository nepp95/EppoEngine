#pragma once

#include "Renderer/Mesh/Material.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Vertex.h"
#include "Renderer/VertexBuffer.h"

namespace Eppo
{
	struct Primitive
	{
		uint32_t FirstVertex = 0;
		uint32_t FirstIndex = 0;
		uint32_t VertexCount = 0;
		uint32_t IndexCount = 0;

		Ref<Material> Material = nullptr;
	};

	class Submesh
	{
	public:
		Submesh(std::string name, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Primitive>& primitives, const glm::mat4& transform);

		[[nodiscard]] Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		[[nodiscard]] Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

		[[nodiscard]] const std::vector<Primitive>& GetPrimitives() const { return m_Primitives; }

		[[nodiscard]] const std::string& GetName() const { return m_Name; }
		[[nodiscard]] const glm::mat4& GetLocalTransform() const { return m_LocalTransform; }

	private:
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		std::vector<Primitive> m_Primitives;

		std::string m_Name;
		glm::mat4 m_LocalTransform;
	};
}
