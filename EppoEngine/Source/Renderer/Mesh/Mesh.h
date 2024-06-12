#pragma once

#include "Asset/Asset.h"
#include "Renderer/Mesh/Material.h"
#include "Renderer/Mesh/Submesh.h"

struct aiNode;

namespace Eppo
{
	class Mesh : public Asset
	{
	public:
		Mesh(const std::filesystem::path& filepath);
		~Mesh() = default;

		const std::vector<Ref<Submesh>>& GetSubmeshes() const  { return m_Submeshes; }
		Ref<Material> GetMaterial(uint32_t index = 0) { return m_Materials[index]; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Mesh; }

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);

	private:
		std::filesystem::path m_Filepath;

		std::vector<Ref<Submesh>> m_Submeshes;
		std::vector<Ref<Material>> m_Materials;
	};
}
