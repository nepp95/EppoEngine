#pragma once

#include "Asset/Asset.h"
#include "Renderer/Mesh/Submesh.h"
#include "Renderer/Mesh/Material.h"
#include "Renderer/Image.h"
#include "Renderer/Vertex.h"

namespace tinygltf
{
	class Model;
	class Node;
	struct Mesh;
}

namespace Eppo
{
	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
		std::vector<Primitive> Primitives;
	};

	class Mesh : public Asset
	{
	public:
		Mesh(const std::filesystem::path& filepath);
		~Mesh() = default;

		const std::vector<Submesh>& GetSubmeshes() const  { return m_Submeshes; }
		const std::vector<Ref<Image>>& GetImages() const { return m_Images; }
		const std::vector<Ref<Material>>& GetMaterials() const { return m_Materials; }

		Ref<Image> GetImage(uint32_t materialIndex) { return m_Images[materialIndex]; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Mesh; }

	private:
		void ProcessNode(const tinygltf::Model& model, const tinygltf::Node& node);
		void ProcessMaterials(const tinygltf::Model& model);
		void ProcessImages(const tinygltf::Model& model);

		MeshData GetVertexData(const tinygltf::Model& model, const tinygltf::Mesh& mesh);

	private:
		std::filesystem::path m_Filepath;

		std::vector<Submesh> m_Submeshes;
		std::vector<Ref<Image>> m_Images;
		std::vector<Ref<Material>> m_Materials;
	};
}
