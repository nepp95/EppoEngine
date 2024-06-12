#include "pch.h"
#include "Submesh.h"

#include "Renderer/Texture.h"
#include "Renderer/Vertex.h"

#include <assimp/scene.h>

namespace Eppo
{
	Submesh::Submesh(aiMesh* mesh, const aiScene* scene)
	{
		EPPO_PROFILE_FUNCTION("Submesh::Submesh");

		// Vertex Buffer
		std::vector<Vertex> vertices;
		vertices.resize(mesh->mNumVertices);

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			const aiVector3D& verts = mesh->mVertices[i];
			vertex.Position = glm::vec3(verts.x, verts.y, verts.z);

			if (mesh->mNormals)
			{
				const aiVector3D& norms = mesh->mNormals[i];
				vertex.Normal = glm::vec3(norms.x, norms.y, norms.z);
			}
			
			if (mesh->mTextureCoords[0])
				vertex.TexCoord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);

			vertices[i] = vertex;
		}

		// Index Buffer
		std::vector<uint32_t> indices;
		indices.resize(mesh->mNumFaces * 3);

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			const aiFace& face = mesh->mFaces[i];

			indices[i * 3 + 0] = face.mIndices[0];
			indices[i * 3 + 1] = face.mIndices[1];
			indices[i * 3 + 2] = face.mIndices[2];
		}

		m_MaterialIndex = mesh->mMaterialIndex;

		m_VertexBuffer = CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
		m_IndexBuffer = CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
		m_VertexArray = CreateRef<VertexArray>(m_VertexBuffer, m_IndexBuffer);

		m_VertexArray->SetLayout({
			{ ShaderDataType::Float3, "inPosition" },
			{ ShaderDataType::Float3, "inNormal" },
			{ ShaderDataType::Float2, "inTexCoord" }
		});
	}
}
