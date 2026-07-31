#include "pch.h"
#include "Renderer/Mesh.h"

#include "Renderer/DeviceManager.h"
#include "Renderer/Vertex.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>
#include <tiny_gltf_v3.h>

#include <execution>

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
            newMat->EmissiveFactor = glm::make_vec3(material.emissive_factor) * GetEmissiveStrength(material);

            m_Materials[i] = newMat;
        }
    }

    auto Mesh::ProcessImages(const tg3_model& model, std::string_view basePath) -> void
    {
        EP_PROFILE_FN("Mesh::ProcessImages")

        const auto device = DeviceManager::Get()->GetDevice();

        // Format look up
        std::unordered_map<uint32_t, nvrhi::Format> imageFormats;
        for (uint32_t i = 0; i < model.materials_count; i++)
        {
            const auto& material = model.materials[i];
            if (const auto texture = material.pbr_metallic_roughness.base_color_texture.index; texture >= 0)
                imageFormats[model.textures[texture].source] = nvrhi::Format::SRGBA8_UNORM;
            if (const auto texture = material.normal_texture.index; texture >= 0)
                imageFormats[model.textures[texture].source] = nvrhi::Format::RGBA8_UNORM;
            if (const auto texture = material.pbr_metallic_roughness.metallic_roughness_texture.index; texture >= 0)
                imageFormats[model.textures[texture].source] = nvrhi::Format::RGBA8_UNORM;
            if (const auto texture = material.occlusion_texture.index; texture >= 0)
                imageFormats[model.textures[texture].source] = nvrhi::Format::RGBA8_UNORM;
            if (const auto texture = material.emissive_texture.index; texture >= 0)
                imageFormats[model.textures[texture].source] = nvrhi::Format::SRGBA8_UNORM;
        }

        m_Images.resize(model.images_count);
        std::vector<nvrhi::CommandListHandle> cmdLists(m_Images.size());

        std::for_each(
            std::execution::par, m_Images.begin(), m_Images.end(),
            [&, model](auto& img)
            {
                int idx = &img - &m_Images[0];
                const auto& image = model.images[idx];

                nvrhi::CommandListParameters params{
                    .enableImmediateExecution = false,
                };

                const auto cmd = device->createCommandList(params);
                cmd->open();

                if (image.image.data == nullptr)
                {
                    if (image.uri.data == nullptr && image.buffer_view > -1)
                    {
                        // Embedded image with buffer view
                        const auto& bufferView = model.buffer_views[image.buffer_view];
                        const auto& buffer = model.buffers[bufferView.buffer];
                        const auto* imageData = &buffer.data.data[bufferView.byte_offset];

                        auto name = std::string(bufferView.name.data, bufferView.name.len);

                        auto format = nvrhi::Format::UNKNOWN;
                        if (auto it = imageFormats.find(idx); it != imageFormats.end())
                            format = it->second;
                        EP_ASSERT(format != nvrhi::Format::UNKNOWN);

                        int width = 0, height = 0, channels = 0;
                        stbi_info_from_memory(imageData, static_cast<int>(buffer.data.count), &width, &height, &channels);
                        EP_ASSERT(width > 0 && height > 0);

                        ImageSpecification spec{
                            .ImageFormat = format,
                            .Width = static_cast<uint32_t>(width),
                            .Height = static_cast<uint32_t>(height),
                            .DebugName = std::format("Image {}", name),
                        };

                        Buffer imageBuffer(buffer.data.count);
                        imageBuffer.Data = const_cast<uint8_t*>(imageData);

                        m_Images[idx] = CreateRef<Image>(spec, imageBuffer, cmd);
                        cmdLists[idx] = cmd;
                    }
                    else
                    {
                        // External image
                        std::string uri(image.uri.data, image.uri.len);
                        std::filesystem::path path = std::filesystem::path(basePath) / uri;

                        nvrhi::Format format = nvrhi::Format::UNKNOWN;
                        if (auto it = imageFormats.find(idx); it != imageFormats.end())
                            format = it->second;
                        EP_ASSERT(format != nvrhi::Format::UNKNOWN);

                        ImageSpecification spec{
                            .ImageFormat = format,
                            .DebugName = std::format("Image {}", uri),
                        };

                        m_Images[idx] = CreateRef<Image>(spec, path, cmd);
                        cmdLists[idx] = cmd;
                    }
                }

                cmd->close();
            }
        );

        // We need the raw pointers, but because of the only strong reference, stored it in a vector as a shared ptr
        std::vector<nvrhi::ICommandList*> rawCmds;
        rawCmds.reserve(cmdLists.size());
        for (const auto& cmd : cmdLists)
            rawCmds.emplace_back(cmd.Get());

        device->executeCommandLists(rawCmds.data(), cmdLists.size());

        const auto& descriptorManager = DeviceManager::Get()->GetRenderer()->GetDescriptorManager();
        std::vector<Ref<BindlessHandle>> imageHandles(m_Images.size());
        for (uint32_t i = 0; i < m_Images.size(); i++)
        {
            if (!m_Images[i])
                continue;

            auto handle = descriptorManager->Register<Image>(m_Images[i]);
            if (handle.Index != std::numeric_limits<uint32_t>::max())
                imageHandles[i] = CreateRef<BindlessHandle>(std::move(handle));
        }

        const auto GetHandle = [&](const int32_t textureIndex) -> Ref<BindlessHandle>
        {
            if (textureIndex < 0)
                return nullptr;
            const int32_t imageIndex = model.textures[textureIndex].source;
            return imageIndex >= 0 ? imageHandles.at(imageIndex) : nullptr;
        };

        for (uint32_t i = 0; i < model.materials_count; i++)
        {
            const auto& source = model.materials[i];
            const auto& material = m_Materials.at(i);
            material->DiffuseMap = GetHandle(source.pbr_metallic_roughness.base_color_texture.index);
            material->NormalMap = GetHandle(source.normal_texture.index);
            material->RoughMetMap = GetHandle(source.pbr_metallic_roughness.metallic_roughness_texture.index);
            material->AOMap = GetHandle(source.occlusion_texture.index);
            material->EmissiveMap = GetHandle(source.emissive_texture.index);
        }
    }
}
