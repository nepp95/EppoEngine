#pragma once

#include "Asset/Asset.h"
#include "Renderer/DescriptorManager.h"
#include "Renderer/Image.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/VertexBuffer.h"

#include <glm/glm.hpp>

#include <limits>

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

    // Axis-aligned bounding box in the mesh's local space — the space the entity's
    // world transform maps from. Computed once at load time (vertices are uploaded
    // to GPU and not retained on CPU), so the renderer can size a selection box
    // without reading back vertex data.
    struct AABB
    {
        glm::vec3 Min{ std::numeric_limits<float>::max() };
        glm::vec3 Max{ std::numeric_limits<float>::lowest() };

        auto Expand(const glm::vec3& p) -> void
        {
            Min = glm::min(Min, p);
            Max = glm::max(Max, p);
        }

        [[nodiscard]] auto GetCenter() const -> glm::vec3 { return (Min + Max) * 0.5f; }
        [[nodiscard]] auto GetHalfExtent() const -> glm::vec3 { return (Max - Min) * 0.5f; }
        [[nodiscard]] auto IsValid() const -> bool { return Min.x <= Max.x; }
    };

    struct Material
    {
        Ref<BindlessHandle> DiffuseMap = nullptr;
        Ref<BindlessHandle> NormalMap = nullptr;
        Ref<BindlessHandle> RoughMetMap = nullptr;

        glm::vec4 BaseColor = glm::vec4(1.0f);
        float Roughness = 1.0f;
        float Metallic = 1.0f;

        // NOTE: This converts a uint32 to a int32 which loses half the range.
        //       Currently this is no issue since our handles won't ever reach that far,
        //       But this might change in the future.
        [[nodiscard]] auto GetDiffuseMapIndex() const -> int32_t { return DiffuseMap ? static_cast<int32_t>(DiffuseMap->Index) : -1; }
        [[nodiscard]] auto GetNormalMapIndex() const -> int32_t { return NormalMap ? static_cast<int32_t>(NormalMap->Index) : -1; }
        [[nodiscard]] auto GetRoughMetMapIndex() const -> int32_t { return RoughMetMap ? static_cast<int32_t>(RoughMetMap->Index) : -1; }
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
        [[nodiscard]] constexpr auto GetBounds() const -> const AABB& { return m_Bounds; }

        [[nodiscard]] constexpr auto IsValid() const -> bool { return m_Valid; }

        // Procedurally builds a Mesh for the given primitive shape. Called by the
        // AssetManager to materialize a primitive asset on first request; subsequent
        // requests are served from the AssetManager's cache, so this only runs once
        // per type. Use AssetManager::GetOrLoadAsset to obtain primitive meshes.
        static auto GenerateMeshPrimitive(MeshPrimitiveType type) -> Ref<Mesh>;

    private:
        auto ProcessNode(const tg3_model& model, const tg3_node& node, const glm::mat4& parentTransform) -> void;
        auto ProcessMesh(const tg3_model& model, const tg3_mesh& mesh, const glm::mat4& localTransform) -> void;
        auto ProcessMaterials(const tg3_model& model) -> void;
        auto ProcessImages(const tg3_model& model, std::string_view basePath) -> void;

    private:
        std::string m_Name;
        std::vector<Submesh> m_Submeshes;
        std::vector<Ref<Material>> m_Materials;
        std::vector<Ref<Image>> m_Images;
        AABB m_Bounds;
        bool m_Valid = true;
    };
}
