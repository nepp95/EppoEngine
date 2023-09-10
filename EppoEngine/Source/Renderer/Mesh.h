#pragma once

#include "Asset/Asset.h"
#include "Renderer/Buffer/IndexBuffer.h"
#include "Renderer/Buffer/VertexBuffer.h"

struct aiMesh;
struct aiNode;
struct aiScene;

namespace Eppo
{
	class Mesh : public Asset
	{
	public:
		Mesh(const std::filesystem::path& filepath);
		~Mesh() = default;

		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Mesh; }

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);

	private:
		std::filesystem::path m_Filepath;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		aiMesh* m_Mesh;
	};
}
