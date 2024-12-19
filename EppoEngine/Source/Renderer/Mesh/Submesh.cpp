#include "pch.h"
#include "Submesh.h"

#include "Renderer/Renderer.h"

namespace Eppo
{
	Submesh::Submesh(std::string name, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Primitive>& primitives, const glm::mat4& transform)
		: m_Primitives(primitives), m_Name(std::move(name)), m_LocalTransform(transform)
	{
		m_VertexBuffer = VertexBuffer::Create((void*)vertices.data(), vertices.size() * sizeof(Vertex));
		m_IndexBuffer = IndexBuffer::Create((void*)indices.data(), indices.size() * sizeof(uint32_t));
	}
}
