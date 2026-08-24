#include "pch.h"
#include "Renderer/Mesh.h"

#include "Core/Application.h"
#include "Renderer/DeviceManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/Vertex.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <tiny_gltf_v3.h>

namespace Eppo
{
    namespace
    {
        auto GetNodeTransform(const tg3_node& node) -> glm::mat4
        {
            if (node.has_matrix)
                return glm::mat4(glm::make_mat4(node.matrix));

            const glm::vec3 translation = glm::make_vec3(node.translation);
            const glm::quat rotation = glm::make_quat(node.rotation);
            const glm::vec3 scale = glm::make_vec3(node.scale);

            return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }

        auto GetEmissiveStrength(const tg3_material& material) -> double
        {
            for (uint32_t i = 0; i < material.ext.extensions_count; i++)
            {
                const auto& extension = material.ext.extensions[i];
                if (std::string_view(extension.name.data, extension.name.len) != "KHR_materials_emissive_strength")
                    continue;
                if (extension.value.type != TG3_VALUE_OBJECT)
                    break;

                for (uint32_t j = 0; j < extension.value.object_count; j++)
                {
                    const auto& member = extension.value.object_data[j];
                    if (std::string_view(member.key.data, member.key.len) != "emissiveStrength")
                        continue;
                    if (member.value.type == TG3_VALUE_REAL)
                        return member.value.real_val;
                    if (member.value.type == TG3_VALUE_INT)
                        return static_cast<double>(member.value.int_val);
                }
                break;
            }

            return 1.0;
        }

        auto GetSamplerAddressMode(const int32_t wrapMode) -> nvrhi::SamplerAddressMode
        {
            switch (wrapMode)
            {
                case TG3_TEXTURE_WRAP_REPEAT:
                    return nvrhi::SamplerAddressMode::Wrap;
                case TG3_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    return nvrhi::SamplerAddressMode::Clamp;
                case TG3_TEXTURE_WRAP_MIRRORED_REPEAT:
                    return nvrhi::SamplerAddressMode::Mirror;
                default:
                    EP_ASSERT(false, "Unsupported glTF texture wrap mode!");
                    return nvrhi::SamplerAddressMode::Wrap;
            }
        }

        auto GenerateTangents(
            std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const uint32_t firstVertex, const uint64_t vertexCount,
            const uint32_t firstIndex, const uint64_t indexCount
        ) -> void
        {
            EP_ASSERT(indexCount % 3 == 0, "Tangent generation requires triangle indices!");

            constexpr float epsilon = 1e-8f;

            std::vector tangentSums(vertexCount, glm::vec3(0.0f));
            std::vector bitangentSums(vertexCount, glm::vec3(0.0f));

            for (uint64_t i = 0; i < indexCount; i += 3)
            {
                const uint32_t i0 = indices[firstIndex + i];
                const uint32_t i1 = indices[firstIndex + i + 1];
                const uint32_t i2 = indices[firstIndex + i + 2];

                EP_ASSERT(i0 < vertexCount && i1 < vertexCount && i2 < vertexCount, "Mesh index exceeds primitive vertex count!");

                const Vertex& v0 = vertices[firstVertex + i0];
                const Vertex& v1 = vertices[firstVertex + i1];
                const Vertex& v2 = vertices[firstVertex + i2];

                const glm::vec3 edge1 = v1.Position - v0.Position;
                const glm::vec3 edge2 = v2.Position - v0.Position;
                const glm::vec2 deltaUV1 = v1.TexCoord - v0.TexCoord;
                const glm::vec2 deltaUV2 = v2.TexCoord - v0.TexCoord;

                const float determinant = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
                if (glm::abs(determinant) <= epsilon)
                    continue;

                const float inverseDeterminant = 1.0f / determinant;
                const glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * inverseDeterminant;
                const glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * inverseDeterminant;

                tangentSums[i0] += tangent;
                tangentSums[i1] += tangent;
                tangentSums[i2] += tangent;
                bitangentSums[i0] += bitangent;
                bitangentSums[i1] += bitangent;
                bitangentSums[i2] += bitangent;
            }

            for (uint64_t i = 0; i < vertexCount; i++)
            {
                Vertex& vertex = vertices[firstVertex + i];

                glm::vec3 normal = vertex.Normal;
                if (glm::dot(normal, normal) <= epsilon)
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                else
                    normal = glm::normalize(normal);

                glm::vec3 tangent = tangentSums[i] - normal * glm::dot(normal, tangentSums[i]);

                if (glm::dot(tangent, tangent) <= epsilon)
                {
                    const glm::vec3 axis = glm::abs(normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                    tangent = glm::normalize(glm::cross(axis, normal));
                }
                else
                {
                    tangent = glm::normalize(tangent);
                }

                const float handedNess = glm::dot(glm::cross(normal, tangent), bitangentSums[i]) < 0.0f ? -1.0f : 1.0f;
                vertex.Tangent = glm::vec4(tangent, handedNess);
            }
        }
    }

    Mesh::Mesh(std::string_view path)
    {
        EP_PROFILE_FN("Mesh::Mesh")

        // Initialize tinygltf
        tg3_parse_options options;
        tg3_error_stack errors;

        // tg3_model_free unconditionally destroys model->arena_, which a failed parse
        // leaves untouched — stack garbage without this.
        tg3_model model{};
        model.default_scene = -1;

        tg3_parse_options_init(&options);
        tg3_error_stack_init(&errors);

        // Parse
        if (auto err = tg3_parse_file(&model, &errors, path.data(), static_cast<uint32_t>(path.size()), &options); err != TG3_OK)
        {
            Log::Error("Failed loading mesh '{}'", path);
            for (uint32_t i = 0; i < errors.count; i++)
                Log::Error("{}", errors.entries[i].message ? errors.entries[i].message : "NULL");

            // A failed parse only partially writes the model, so its counts are garbage.
            m_Valid = false;
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
            return;
        }

        ProcessMaterials(model);

        auto pos = path.find_last_of("\\/");
        m_Name = pos == std::string_view::npos ? path : path.substr(pos + 1);
        ProcessImages(model, pos == std::string_view::npos ? "" : path.substr(0, pos + 1));

        if (model.scenes_count > 0)
        {
            const int32_t sceneIndex = model.default_scene >= 0 ? model.default_scene : 0;
            EP_ASSERT(sceneIndex < static_cast<int32_t>(model.scenes_count));

            const auto& scene = model.scenes[sceneIndex];
            for (uint32_t i = 0; i < scene.nodes_count; i++)
            {
                const int32_t nodeIndex = scene.nodes[i];
                EP_ASSERT(nodeIndex >= 0 && nodeIndex < static_cast<int32_t>(model.nodes_count));
                ProcessNode(model, model.nodes[nodeIndex], glm::mat4(1.0f));
            }
        }
        else
        {
            std::vector<bool> childNodes(model.nodes_count, false);
            for (uint32_t i = 0; i < model.nodes_count; i++)
            {
                const auto& node = model.nodes[i];
                for (uint32_t childIndex = 0; childIndex < node.children_count; childIndex++)
                {
                    const int32_t child = node.children[childIndex];
                    EP_ASSERT(child >= 0 && child < static_cast<int32_t>(model.nodes_count));
                    childNodes[child] = true;
                }
            }

            for (uint32_t i = 0; i < model.nodes_count; i++)
            {
                if (!childNodes[i])
                    ProcessNode(model, model.nodes[i], glm::mat4(1.0f));
            }
        }

        // Free
        tg3_model_free(&model);
        tg3_error_stack_free(&errors);
    }

    auto Mesh::GenerateMeshPrimitive(const MeshPrimitiveType type) -> Ref<Mesh>
    {
        constexpr auto MeshPrimitiveTypeToString = [](const MeshPrimitiveType type) -> std::string
        {
            if (type == MeshPrimitiveType::Cone)
                return "Cone";
            if (type == MeshPrimitiveType::Cube)
                return "Cube";
            if (type == MeshPrimitiveType::Cylinder)
                return "Cylinder";
            if (type == MeshPrimitiveType::Sphere)
                return "Sphere";
            if (type == MeshPrimitiveType::Capsule)
                return "Capsule";
            return "Unknown";
        };

        const auto vb = VertexBuffer::GeneratePrimitive(type);
        const auto ib = IndexBuffer::GeneratePrimitive(type);

        Ref<Material> material = CreateRef<Material>();

        Primitive primitive{
            .VertexCount = vb->GetSize() / sizeof(Vertex),
            .IndexCount = ib->GetIndexCount(),
            .Material = material,
        };

        Submesh submesh{
            .Name = MeshPrimitiveTypeToString(type),
            .VertexBuffer = vb,
            .IndexBuffer = ib,
            .Primitives = { primitive },
            .LocalTransform = glm::mat4(1.0f),
        };

        Ref<Mesh> mesh = CreateRef<Mesh>();
        mesh->m_Submeshes.emplace_back(submesh);
        mesh->m_Materials.emplace_back(material);
        mesh->m_Name = MeshPrimitiveTypeToString(type);

        // Unit primitives: all fit within a ±1 cube except the capsule, whose
        // hemispheres extend to ±2 on Y (radius 1, hemisphere centers at ±1).
        mesh->m_Bounds.Min = glm::vec3(-1.0f);
        mesh->m_Bounds.Max = glm::vec3(1.0f);
        if (type == MeshPrimitiveType::Capsule)
        {
            mesh->m_Bounds.Min.y = -2.0f;
            mesh->m_Bounds.Max.y = 2.0f;
        }

        return mesh;
    }

    auto Mesh::ProcessNode(const tg3_model& model, const tg3_node& node, const glm::mat4& parentTransform) -> void
    {
        EP_PROFILE_FN("Mesh::ProcessNode")

        const glm::mat4 localTransform = parentTransform * GetNodeTransform(node);

        if (const int32_t meshIndex = node.mesh; meshIndex > -1)
        {
            ProcessMesh(model, model.meshes[meshIndex], localTransform);
        }

        for (uint32_t i = 0; i < node.children_count; i++)
        {
            const int32_t childIndex = node.children[i];
            EP_ASSERT(childIndex >= 0 && childIndex < static_cast<int32_t>(model.nodes_count));
            ProcessNode(model, model.nodes[childIndex], localTransform);
        }
    }

    auto Mesh::ProcessMesh(const tg3_model& model, const tg3_mesh& mesh, const glm::mat4& localTransform) -> void
    {
        EP_PROFILE_FN("Mesh::ProcessMesh")

        Submesh data{
            .Name = std::string(mesh.name.data, mesh.name.len),
            .LocalTransform = localTransform,
        };

        data.Primitives.reserve(mesh.primitives_count);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (uint32_t i = 0; i < mesh.primitives_count; i++)
        {
            Primitive& p = data.Primitives.emplace_back();
            p.FirstVertex = static_cast<uint32_t>(vertices.size());
            p.FirstIndex = static_cast<uint32_t>(indices.size());

            uint64_t vertexCount = 0;

            const auto& mp = mesh.primitives[i];
            const float* positionData = nullptr;
            const float* normalData = nullptr;
            const float* texCoordData = nullptr;
            const float* tangentData = nullptr;

            for (uint32_t j = 0; j < mp.attributes_count; j++)
            {
                const auto& attribute = mp.attributes[j];
                if (std::strncmp(attribute.key.data, "POSITION", 8) == 0)
                {
                    EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
                    const auto& accessor = model.accessors[attribute.value];
                    const auto& bufferView = model.buffer_views[accessor.buffer_view];
                    positionData = reinterpret_cast<const float*>(
                        &model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]
                    );
                    vertexCount = accessor.count;
                }

                if (std::strncmp(attribute.key.data, "NORMAL", 6) == 0)
                {
                    EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
                    const auto& accessor = model.accessors[attribute.value];
                    const auto& bufferView = model.buffer_views[accessor.buffer_view];
                    normalData = reinterpret_cast<const float*>(
                        &model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]
                    );
                }

                if (std::strncmp(attribute.key.data, "TEXCOORD_0", 10) == 0)
                {
                    EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
                    const auto& accessor = model.accessors[attribute.value];
                    const auto& bufferView = model.buffer_views[accessor.buffer_view];
                    texCoordData = reinterpret_cast<const float*>(
                        &model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]
                    );
                }

                if (std::strncmp(attribute.key.data, "TANGENT", 7) == 0)
                {
                    EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
                    const auto& accessor = model.accessors[attribute.value];
                    const auto& bufferView = model.buffer_views[accessor.buffer_view];
                    tangentData = reinterpret_cast<const float*>(
                        &model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]
                    );
                }
            }

            const auto vtxOffset = vertices.size();
            vertices.resize(vtxOffset + vertexCount);

            for (size_t j = 0; j < vertexCount; j++)
            {
                Vertex& vertex = vertices[vtxOffset + j];
                vertex.Position = glm::make_vec3(&positionData[j * 3]);
                vertex.Normal = glm::make_vec3(&normalData[j * 3]);
                vertex.TexCoord = glm::make_vec2(&texCoordData[j * 2]);

                if (tangentData)
                    vertex.Tangent = glm::make_vec4(&tangentData[j * 4]);
            }

            // Indices
            const auto& accessor = model.accessors[mp.indices];
            const auto& bufferView = model.buffer_views[accessor.buffer_view];
            const auto& buffer = model.buffers[bufferView.buffer];

            indices.reserve(accessor.count + indices.size());

            if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const auto* indexData = reinterpret_cast<const uint32_t*>(&buffer.data.data[accessor.byte_offset + bufferView.byte_offset]);
                for (uint64_t j = 0; j < accessor.count; j++)
                    indices.emplace_back(indexData[j]);
            }
            else if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const auto* indexData = reinterpret_cast<const uint16_t*>(&buffer.data.data[accessor.byte_offset + bufferView.byte_offset]);
                for (uint64_t j = 0; j < accessor.count; j++)
                    indices.emplace_back(indexData[j]);
            }
            else if (accessor.component_type == TG3_COMPONENT_TYPE_BYTE)
            {
                const auto* indexData = reinterpret_cast<const uint8_t*>(&buffer.data.data[accessor.byte_offset + bufferView.byte_offset]);
                for (uint64_t j = 0; j < accessor.count; j++)
                    indices.emplace_back(indexData[j]);
            }
            else
            {
                EP_ASSERT(false);
            }

            p.VertexCount = vertexCount;
            p.IndexCount = accessor.count;

            if (!tangentData)
                GenerateTangents(vertices, indices, p.FirstVertex, p.VertexCount, p.FirstIndex, p.IndexCount);

            if (mp.material != -1)
                p.Material = m_Materials.at(mp.material);
        }

        data.VertexBuffer = CreateRef<VertexBuffer>(vertices.data(), static_cast<uint64_t>(vertices.size() * sizeof(Vertex)));
        data.IndexBuffer = CreateRef<IndexBuffer>(indices.data(), static_cast<uint64_t>(indices.size() * sizeof(uint32_t)));

        // Accumulate mesh-local bounds: vertices are in submesh space, so
        // transform by the submesh's LocalTransform to reach the space the
        // entity's world transform maps from.
        for (const auto& v : vertices)
            m_Bounds.Expand(glm::vec3(localTransform * glm::vec4(v.Position, 1.0f)));

        m_Submeshes.emplace_back(std::move(data));
    }

    auto Mesh::ProcessMaterials(const tg3_model& model) -> void
    {
        EP_PROFILE_FN("Mesh::ProcessMaterials")

        m_Materials.resize(model.materials_count);
        for (uint32_t i = 0; i < model.materials_count; i++)
        {
            const auto& material = model.materials[i];
            auto newMat = CreateRef<Material>();

            newMat->BaseColor = glm::make_vec4(material.pbr_metallic_roughness.base_color_factor);
            newMat->Roughness = static_cast<float>(material.pbr_metallic_roughness.roughness_factor);
            newMat->Metallic = static_cast<float>(material.pbr_metallic_roughness.metallic_factor);
            newMat->NormalScale = static_cast<float>(material.normal_texture.scale);
            newMat->EmissiveFactor = glm::make_vec3(material.emissive_factor) * GetEmissiveStrength(material);

            if (const int32_t textureIndex = material.pbr_metallic_roughness.base_color_texture.index; textureIndex >= 0)
            {
                if (model.textures[textureIndex].sampler >= 0)
                {
                    const auto& sampler = model.samplers[model.textures[textureIndex].sampler];
                    newMat->DiffuseSampler = DeviceManager::Get()->GetRenderer()->GetSampler(
                        SamplerSpecification{
                            .AddressModeU = GetSamplerAddressMode(sampler.wrap_s),
                            .AddressModeV = GetSamplerAddressMode(sampler.wrap_t),
                            .MinFilter = sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
                            .MagFilter = sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST,
                            .MipFilter = sampler.min_filter == TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                                sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
                            .MaxAnisotropy = sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR &&
                                    sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST
                                ? 16.0f
                                : 1.0f,
                        }
                    );
                }
                else
                {
                    newMat->DiffuseSampler = DeviceManager::Get()->GetRenderer()->GetSampler({});
                }
            }

            if (const int32_t textureIndex = material.normal_texture.index; textureIndex >= 0)
            {
                if (model.textures[textureIndex].sampler >= 0)
                {
                    const auto& sampler = model.samplers[model.textures[textureIndex].sampler];
                    newMat->NormalSampler = DeviceManager::Get()->GetRenderer()->GetSampler(
                        SamplerSpecification{
                            .AddressModeU = GetSamplerAddressMode(sampler.wrap_s),
                            .AddressModeV = GetSamplerAddressMode(sampler.wrap_t),
                            .MinFilter = sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
                            .MagFilter = sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST,
                            .MipFilter = sampler.min_filter == TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                                sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
                            .MaxAnisotropy = sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR &&
                                    sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST
                                ? 16.0f
                                : 1.0f,
                        }
                    );
                }
                else
                {
                    newMat->NormalSampler = DeviceManager::Get()->GetRenderer()->GetSampler({});
                }
            }

            if (const int32_t textureIndex = material.pbr_metallic_roughness.metallic_roughness_texture.index; textureIndex >= 0)
            {
                if (model.textures[textureIndex].sampler >= 0)
                {
                    const auto& sampler = model.samplers[model.textures[textureIndex].sampler];
                    newMat->RoughMetSampler = DeviceManager::Get()->GetRenderer()->GetSampler(
                        SamplerSpecification{
                            .AddressModeU = GetSamplerAddressMode(sampler.wrap_s),
                            .AddressModeV = GetSamplerAddressMode(sampler.wrap_t),
                            .MinFilter = sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
                            .MagFilter = sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST,
                            .MipFilter = sampler.min_filter == TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                                sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
                            .MaxAnisotropy = sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR &&
                                    sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST
                                ? 16.0f
                                : 1.0f,
                        }
                    );
                }
                else
                {
                    newMat->RoughMetSampler = DeviceManager::Get()->GetRenderer()->GetSampler({});
                }
            }

            if (const int32_t textureIndex = material.occlusion_texture.index; textureIndex >= 0)
            {
                if (model.textures[textureIndex].sampler >= 0)
                {
                    const auto& sampler = model.samplers[model.textures[textureIndex].sampler];
                    newMat->AOSampler = DeviceManager::Get()->GetRenderer()->GetSampler(
                        SamplerSpecification{
                            .AddressModeU = GetSamplerAddressMode(sampler.wrap_s),
                            .AddressModeV = GetSamplerAddressMode(sampler.wrap_t),
                            .MinFilter = sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
                            .MagFilter = sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST,
                            .MipFilter = sampler.min_filter == TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                                sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
                            .MaxAnisotropy = sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR &&
                                    sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST
                                ? 16.0f
                                : 1.0f,
                        }
                    );
                }
                else
                {
                    newMat->AOSampler = DeviceManager::Get()->GetRenderer()->GetSampler({});
                }
            }

            if (const int32_t textureIndex = material.emissive_texture.index; textureIndex >= 0)
            {
                if (model.textures[textureIndex].sampler >= 0)
                {
                    const auto& sampler = model.samplers[model.textures[textureIndex].sampler];
                    newMat->EmissiveSampler = DeviceManager::Get()->GetRenderer()->GetSampler(
                        SamplerSpecification{
                            .AddressModeU = GetSamplerAddressMode(sampler.wrap_s),
                            .AddressModeV = GetSamplerAddressMode(sampler.wrap_t),
                            .MinFilter = sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST &&
                                sampler.min_filter != TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
                            .MagFilter = sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST,
                            .MipFilter = sampler.min_filter == TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
                                sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,
                            .MaxAnisotropy = sampler.min_filter == TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR &&
                                    sampler.mag_filter != TG3_TEXTURE_FILTER_NEAREST
                                ? 16.0f
                                : 1.0f,
                        }
                    );
                }
                else
                {
                    newMat->EmissiveSampler = DeviceManager::Get()->GetRenderer()->GetSampler({});
                }
            }

            m_Materials[i] = newMat;
        }
    }

    auto Mesh::ProcessImages(const tg3_model& model, std::string_view basePath) -> void
    {
        EP_PROFILE_FN("Mesh::ProcessImages")

        const auto device = DeviceManager::Get()->GetDevice();

        struct MaterialImages
        {
            int32_t Diffuse = -1;
            int32_t Normal = -1;
            int32_t RoughMet = -1;
            int32_t AO = -1;
            int32_t Emissive = -1;
        };

        struct ImageProperties
        {
            nvrhi::Format ImageFormat = nvrhi::Format::UNKNOWN;
            MipGenerationMode MipMode = MipGenerationMode::None;
        };

        std::unordered_map<uint32_t, ImageProperties> imageProperties;
        std::vector<MaterialImages> materialImages(model.materials_count);
        for (uint32_t i = 0; i < model.materials_count; i++)
        {
            const auto& material = model.materials[i];
            if (const auto texture = material.pbr_metallic_roughness.base_color_texture.index; texture >= 0)
            {
                imageProperties[model.textures[texture].source] =
                    ImageProperties{ .ImageFormat = nvrhi::Format::SRGBA8_UNORM, .MipMode = MipGenerationMode::ColorSRGB };
                materialImages[i].Diffuse = model.textures[texture].source;
            }
            if (const auto texture = material.normal_texture.index; texture >= 0)
            {
                imageProperties[model.textures[texture].source] =
                    ImageProperties{ .ImageFormat = nvrhi::Format::RGBA8_UNORM, .MipMode = MipGenerationMode::NormalMap };
                materialImages[i].Normal = model.textures[texture].source;
            }
            if (const auto texture = material.pbr_metallic_roughness.metallic_roughness_texture.index; texture >= 0)
            {
                imageProperties[model.textures[texture].source] =
                    ImageProperties{ .ImageFormat = nvrhi::Format::RGBA8_UNORM, .MipMode = MipGenerationMode::Linear };
                materialImages[i].RoughMet = model.textures[texture].source;
            }
            if (const auto texture = material.occlusion_texture.index; texture >= 0)
            {
                imageProperties[model.textures[texture].source] =
                    ImageProperties{ .ImageFormat = nvrhi::Format::RGBA8_UNORM, .MipMode = MipGenerationMode::Linear };
                materialImages[i].AO = model.textures[texture].source;
            }
            if (const auto texture = material.emissive_texture.index; texture >= 0)
            {
                imageProperties[model.textures[texture].source] =
                    ImageProperties{ .ImageFormat = nvrhi::Format::SRGBA8_UNORM, .MipMode = MipGenerationMode::ColorSRGB };
                materialImages[i].Emissive = model.textures[texture].source;
            }
        }

        m_Images.resize(model.images_count);
        std::vector<nvrhi::CommandListHandle> cmdLists(m_Images.size());
        std::vector<TaskId> imageTasks;
        imageTasks.reserve(m_Images.size());
        const auto& threadPool = Application::Get().GetThreadPool();

        for (size_t i = 0; i < m_Images.size(); i++)
        {
            const auto& image = model.images[i];

            if (image.image.data == nullptr)
            {
                if (image.uri.data == nullptr && image.buffer_view > -1)
                {
                    // Embedded image with buffer view
                    const auto& bufferView = model.buffer_views[image.buffer_view];
                    const auto& buffer = model.buffers[bufferView.buffer];
                    EP_ASSERT(bufferView.byte_offset + bufferView.byte_length <= buffer.data.count);
                    const auto* imageData = &buffer.data.data[bufferView.byte_offset];

                    auto name = std::string(bufferView.name.data, bufferView.name.len);

                    const auto properties = imageProperties.find(static_cast<uint32_t>(i));
                    EP_ASSERT(properties != imageProperties.end());

                    ImageSpecification spec{
                        .ImageFormat = properties->second.ImageFormat,
                        .MipMode = properties->second.MipMode,
                        .DebugName = std::format("Image {}", name),
                    };

                    cmdLists[i] = device->createCommandList(nvrhi::CommandListParameters{ .enableImmediateExecution = false });
                    const Buffer imageBuffer(const_cast<uint8_t*>(imageData), bufferView.byte_length);
                    m_Images[i] = Image::Create(spec, ImageSource(imageBuffer), cmdLists[i]);
                }
                else if (image.uri.data)
                {
                    // External image
                    std::string uri(image.uri.data, image.uri.len);
                    std::filesystem::path path = std::filesystem::path(basePath) / uri;

                    const auto properties = imageProperties.find(static_cast<uint32_t>(i));
                    EP_ASSERT(properties != imageProperties.end());

                    ImageSpecification spec{
                        .ImageFormat = properties->second.ImageFormat,
                        .MipMode = properties->second.MipMode,
                        .DebugName = std::format("Image {}", uri),
                    };

                    cmdLists[i] = device->createCommandList(nvrhi::CommandListParameters{ .enableImmediateExecution = false });
                    m_Images[i] = Image::Create(spec, ImageSource(path), cmdLists[i]);
                }
            }

            if (m_Images[i])
                imageTasks.emplace_back(m_Images[i]->GetLoadTaskId());
        }

        if (imageTasks.empty())
            return;

        threadPool->QueueTaskWithDependencies(
            std::format("Loading mesh {}", m_Name),
            []() -> void
            {
            },
            [images = m_Images, cmdLists = std::move(cmdLists), materials = m_Materials,
             materialImages = std::move(materialImages)](const TaskStatus status)
            {
                if (status != TaskStatus::Completed)
                    return;

                Renderer::Submit(
                    [images, cmdLists, materials, materialImages]()
                    {
                        const auto device = DeviceManager::Get()->GetDevice();
                        std::vector<nvrhi::ICommandList*> rawCmds;
                        rawCmds.reserve(cmdLists.size());
                        for (uint32_t i = 0; i < images.size(); i++)
                        {
                            if (images[i] && images[i]->GetTexture())
                                rawCmds.emplace_back(cmdLists[i].Get());
                        }

                        if (!rawCmds.empty())
                            device->executeCommandLists(rawCmds.data(), rawCmds.size());

                        const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
                        std::vector<Ref<BindlessHandle>> imageHandles(images.size());
                        for (uint32_t i = 0; i < images.size(); i++)
                        {
                            if (!images[i] || !images[i]->GetTexture())
                                continue;

                            auto handle = descriptorManager->Register<Image>(images[i]);
                            if (handle.Index != std::numeric_limits<uint32_t>::max())
                                imageHandles[i] = CreateRef<BindlessHandle>(std::move(handle));
                        }

                        const auto GetHandle = [&imageHandles](const int32_t imageIndex) -> Ref<BindlessHandle>
                        {
                            return imageIndex >= 0 ? imageHandles.at(imageIndex) : nullptr;
                        };

                        for (uint32_t i = 0; i < materials.size(); i++)
                        {
                            const auto& material = materials[i];
                            const auto& source = materialImages[i];
                            material->DiffuseMap = GetHandle(source.Diffuse);
                            material->NormalMap = GetHandle(source.Normal);
                            material->RoughMetMap = GetHandle(source.RoughMet);
                            material->AOMap = GetHandle(source.AO);
                            material->EmissiveMap = GetHandle(source.Emissive);
                        }

                        for (const auto& image : images)
                        {
                            if (image && image->GetTexture())
                                image->IsLoaded.store(true, std::memory_order_release);
                        }
                    }
                );
            },
            imageTasks
        );
    }
}
