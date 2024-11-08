#include "pch.h"
#include "Submesh.h"

namespace Eppo
{
	Submesh::Submesh(const std::string& name, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Primitive>& primitives, const glm::mat4& transform)
		: m_Name(name), m_LocalTransform(transform), m_Primitives(primitives)
	{
		m_VertexBuffer = CreateRef<VertexBuffer>((void*)vertices.data(), vertices.size() * sizeof(Vertex));
		m_IndexBuffer = CreateRef<IndexBuffer>((void*)indices.data(), indices.size() * sizeof(uint32_t));
	}
}
