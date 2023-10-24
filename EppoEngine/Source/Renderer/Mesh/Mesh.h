#pragma once

#include "Asset/Asset.h"
#include "Renderer/Mesh/Submesh.h"

namespace Eppo
{
	class Mesh : public Asset
	{
	public:
		Mesh(const std::filesystem::path& filepath);
		~Mesh() = default;

		// TODO: Refactor material system
		struct MeshMaterial
		{
			std::string Name;
			glm::vec3 DiffuseColor;
			float Roughness;
		};

		const std::vector<Submesh>& GetSubmeshes() const  { return m_Submeshes; }
		const MeshMaterial& GetMaterial(uint32_t index = 0) const { return m_Materials[index]; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Mesh; }

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);

	private:
		std::filesystem::path m_Filepath;

		std::vector<Submesh> m_Submeshes;
		std::vector<MeshMaterial> m_Materials;
	};
}
