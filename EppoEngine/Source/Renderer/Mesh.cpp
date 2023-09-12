#include "pch.h"
#include "Mesh.h"

#include "Renderer/Vertex.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Eppo
{
	Mesh::Mesh(const std::filesystem::path& filepath)
		: m_Filepath(filepath)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath.string(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			EPPO_ERROR("Could not load mesh '{}'", filepath.string());
			EPPO_ERROR("Assimp: {}", importer.GetErrorString());
			return;
		}

		m_Mesh = scene->mMeshes[0];

		std::vector<MeshVertex> vertices;
		for (uint32_t i = 0; i < m_Mesh->mNumVertices; i++)
		{
			MeshVertex vertex;
			vertex.Position = { m_Mesh->mVertices[i].x, m_Mesh->mVertices[i].y, m_Mesh->mVertices[i].z };
			vertex.Normal = { m_Mesh->mNormals[i].x, m_Mesh->mNormals[i].y, m_Mesh->mNormals[i].z };

			if (m_Mesh->mTextureCoords[0])
				vertex.TexCoord = { m_Mesh->mTextureCoords[0][i].x, m_Mesh->mTextureCoords[0][i].y };

			vertices.push_back(vertex);
		}

		std::vector<uint32_t> indices;
		for (uint32_t i = 0; i < m_Mesh->mNumFaces; i++)
		{
			aiFace face = m_Mesh->mFaces[i];
			
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		m_VertexBuffer = CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(MeshVertex));
		m_IndexBuffer = CreateRef<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t));
	}

	void Mesh::ProcessNode(aiNode* node, const aiScene* scene)
	{
	}
}
