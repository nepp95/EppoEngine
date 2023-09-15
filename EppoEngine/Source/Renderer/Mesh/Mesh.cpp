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

		ProcessNode(scene->mRootNode, scene);
	}

	void Mesh::ProcessNode(aiNode* node, const aiScene* scene)
	{
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

			std::string directory = m_Filepath.string();
			directory = directory.substr(0, directory.find_last_of('/'));

			m_Submeshes.push_back(Submesh(mesh, scene, directory));
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			ProcessNode(node->mChildren[i], scene);
	}
}
