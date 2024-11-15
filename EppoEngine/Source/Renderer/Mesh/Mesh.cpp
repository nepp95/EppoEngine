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
		EPPO_PROFILE_FUNCTION("Mesh::Mesh");

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath.string(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			EPPO_ERROR("Could not load mesh '{}'", filepath);
			EPPO_ERROR("Assimp: {}", importer.GetErrorString());
			return;
		}

		ProcessNode(scene->mRootNode, scene);

		// Materials
		if (scene->HasMaterials())
		{
			EPPO_TRACE("Mesh '{}' has {} materials", filepath, scene->mNumMaterials);

			m_Materials.resize(scene->mNumMaterials);
			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				aiMaterial* aiMaterial = scene->mMaterials[i];
				aiString name = aiMaterial->GetName();

				MeshMaterial& material = m_Materials[i];
				material.Name = name.C_Str();

				aiColor3D diffuseColor;
				aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
				material.DiffuseColor = { diffuseColor.r, diffuseColor.g, diffuseColor.b };

				float shininess;
				if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
					shininess = 80.0f; // Default
				material.Roughness = 1.0f - glm::sqrt(shininess / 100.0f);
			}
		}
	}

	void Mesh::ProcessNode(aiNode* node, const aiScene* scene)
	{
		EPPO_PROFILE_FUNCTION("Mesh::ProcessNode");

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Submeshes.push_back(Submesh(mesh, scene));
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			ProcessNode(node->mChildren[i], scene);
	}
}
