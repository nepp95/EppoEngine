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
		std::vector<MeshVertex> vertices;
		vertices.resize(mesh->mNumVertices);

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			MeshVertex vertex;
			vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
			vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
			
			if (mesh->mTextureCoords[0])
				vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

			vertices[i] = vertex;
		}

		// Index Buffer
		std::vector<uint32_t> indices;
		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];

			uint32_t offset = indices.size();
			indices.resize(offset + face.mNumIndices);
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				indices[offset + j] = face.mIndices[j];
		}

		m_MaterialIndex = mesh->mMaterialIndex;

		m_VertexBuffer = CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(MeshVertex));
		m_IndexBuffer = CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
	}
}
