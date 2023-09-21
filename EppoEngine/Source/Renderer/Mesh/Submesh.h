#pragma once

#include "Renderer/Buffer/IndexBuffer.h"
#include "Renderer/Buffer/VertexBuffer.h"

struct aiMesh;
struct aiNode;
struct aiScene;

namespace Eppo
{
	class Submesh
	{
	public:
		Submesh(aiMesh* mesh, const aiScene* scene);

		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }
		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }

	private:
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
	};
}
