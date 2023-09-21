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
			EPPO_ERROR("Could not load mesh '{}'", filepath.string());
			EPPO_ERROR("Assimp: {}", importer.GetErrorString());
			return;
		}

		ProcessNode(scene->mRootNode, scene);

		// Materials
		if (scene->HasMaterials())
		{
			EPPO_TRACE("Mesh '{}' has {} materials", filepath.string(), scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				aiMaterial* material = scene->mMaterials[i];
				aiString filepath;

				material->GetTexture(aiTextureType_DIFFUSE, 0, &filepath);
#if 0
				EPPO_TRACE("Material: {}", material->GetName().C_Str());
				for (uint32_t j = 0; j < material->mNumProperties; j++)
				{
					EPPO_TRACE("\tProperty {}:", j);
					aiMaterialProperty* mp = material->mProperties[j];

					EPPO_TRACE("\t\tmKey: {}", mp->mKey.C_Str());
					std::string data(mp->mData, mp->mDataLength);
					EPPO_TRACE("\t\tmData: {}", data);
					EPPO_TRACE("\t\tmIndex: {}", mp->mIndex);
					EPPO_TRACE("\t\tmType: {}", mp->mType);
					EPPO_TRACE("\t\tmSemantic: {}", mp->mSemantic);
				}
#endif

				material->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), filepath);
				EPPO_TRACE("{}", filepath.C_Str());

				const aiTexture* texture = scene->GetEmbeddedTexture(filepath.C_Str());
				EPPO_ASSERT(texture);


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
