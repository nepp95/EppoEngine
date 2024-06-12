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
		const aiScene* scene = importer.ReadFile(filepath.string(),
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder | aiProcess_GenSmoothNormals | 
			aiProcess_JoinIdenticalVertices | aiProcess_ValidateDataStructure | aiProcess_OptimizeGraph | aiProcess_OptimizeMeshes
		);

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

				Ref<Material> material = CreateRef<Material>();
				material->Name = name.C_Str();

				// Colors
				aiColor3D ambientColor;
				if (aiMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor) == AI_SUCCESS)
					material->AmbientColor = glm::vec3(ambientColor.r, ambientColor.g, ambientColor.b);

				aiColor3D diffuseColor;
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS)
					material->DiffuseColor = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
					
				aiColor3D specularColor;
				if (aiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS)
					material->SpecularColor = glm::vec3(specularColor.r, specularColor.g, specularColor.b);

				// Textures
				for (uint32_t i = 0; i < aiMaterial->GetTextureCount(aiTextureType_DIFFUSE); i++)
				{
					aiString filename;
					aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &filename);

					auto texture = scene->GetEmbeddedTexture(filename.C_Str());
					if (texture)
						material->DiffuseTexture = CreateRef<Texture>(texture->pcData, texture->mWidth);
					else
						material->DiffuseTexture = CreateRef<Texture>(TextureSpecification(filename.C_Str()));
				}

				for (uint32_t i = 0; i < aiMaterial->GetTextureCount(aiTextureType_NORMALS); i++)
				{
					aiString filename;
					aiMaterial->GetTexture(aiTextureType_NORMALS, i, &filename);

					auto texture = scene->GetEmbeddedTexture(filename.C_Str());
					if (texture)
						material->NormalTexture = CreateRef<Texture>(texture->pcData, texture->mWidth);
				}
				
				// Roughness
				float shininess;
				if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
					shininess = 80.0f; // Default
				//material->Roughness = 1.0f - glm::sqrt(shininess / 100.0f);
				material->Roughness = shininess;

				m_Materials[i] = material;
			}
		}
	}

	void Mesh::ProcessNode(aiNode* node, const aiScene* scene)
	{
		EPPO_PROFILE_FUNCTION("Mesh::ProcessNode");

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			m_Submeshes.push_back(CreateRef<Submesh>(mesh, scene));
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			ProcessNode(node->mChildren[i], scene);
	}
}
