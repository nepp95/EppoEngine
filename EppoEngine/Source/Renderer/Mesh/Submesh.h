#pragma once

#include "Renderer/IndexBuffer.h"
#include "Renderer/VertexArray.h"
#include "Renderer/VertexBuffer.h"

struct aiMesh;
struct aiScene;

namespace Eppo
{
	class Submesh
	{
	public:
		Submesh(aiMesh* mesh, const aiScene* scene);

		Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }
		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

		uint32_t GetMaterialIndex() const { return m_MaterialIndex; }

	private:
		Ref<VertexArray> m_VertexArray;
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		uint32_t m_MaterialIndex = 0;
	};
}
