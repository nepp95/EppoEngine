#pragma once

#include "Asset/Asset.h"
#include "Renderer/Image.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/Vertex.h"
#include "Renderer/VertexBuffer.h"

#include <glm/glm.hpp>

struct tg3_mesh;
struct tg3_model;
struct tg3_node;

namespace Eppo
{
	enum class MeshPrimitiveType
	{
		Cone = 1,
		Cube = 2,
		Cylinder = 3,
		Sphere = 4,
		Capsule = 5,
	};

	struct Material
	{
		int32_t DiffuseMapIndex = -1;
		int32_t NormalMapIndex = -1;
		int32_t RoughMetMapIndex = -1;

		glm::vec4 BaseColor = glm::vec4(1.0f);
		float Roughness = 1.0f;
		float Metallic = 1.0f;
	};

	struct Primitive
	{
		uint32_t FirstVertex = 0;
		uint32_t FirstIndex = 0;
		uint64_t VertexCount = 0;
		uint64_t IndexCount = 0;
		Ref<Material> Material = nullptr;
	};

	struct Submesh
	{
		std::string Name;
		Ref<VertexBuffer> VertexBuffer = nullptr;
		Ref<IndexBuffer> IndexBuffer = nullptr;
		std::vector<Primitive> Primitives;
		glm::mat4 LocalTransform;
	};

	class Mesh : public Asset
	{
	public:
		Mesh() = default;
		Mesh(std::string_view path);

		static auto GetStaticType() -> AssetType { return AssetType::Mesh; }

		[[nodiscard]] constexpr auto GetName() const -> const std::string& { return m_Name; }
		[[nodiscard]] constexpr auto GetSubmeshes() const -> const std::vector<Submesh>& { return m_Submeshes; }
		[[nodiscard]] auto GetMaterial(uint32_t materialIndex) const -> const Ref<Material>& { return m_Materials.at(materialIndex); }
		[[nodiscard]] constexpr auto GetImages() const -> const std::vector<Ref<Image>>& { return m_Images; }
		[[nodiscard]] auto GetImage(uint32_t imageIndex) const -> const Ref<Image>& { return m_Images.at(imageIndex); }

		static auto CreateMeshPrimitive(MeshPrimitiveType type) -> Ref<Mesh>;

	private:
		auto ProcessNode(const tg3_model& model, const tg3_node& node) -> void;
		auto ProcessMesh(const tg3_model& model, const tg3_mesh& mesh, const glm::mat4& localTransform) -> void;
		auto ProcessMaterials(const tg3_model& model) -> void;
		auto ProcessImages(const tg3_model& model, std::string_view basePath) -> void;

	private:
		std::string m_Name;
		std::vector<Submesh> m_Submeshes;
		std::vector<Ref<Material>> m_Materials;
		std::vector<Ref<Image>> m_Images;
	};
}