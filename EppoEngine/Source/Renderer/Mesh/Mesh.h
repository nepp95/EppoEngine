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

		const std::vector<Submesh>& GetSubmeshes() const  { return m_Submeshes; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Mesh; }

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);

	private:
		std::filesystem::path m_Filepath;

		std::vector<Submesh> m_Submeshes;
	};
}
