#include "pch.h"
#include "Renderer/Mesh.h"

#include "Renderer/DeviceManager.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>
#include <tiny_gltf_v3.h>

namespace Eppo
{
	Mesh::Mesh(std::string_view path)
	{
		EP_PROFILE_FN("Mesh::Mesh")

		// Initialize tinygltf
		tg3_parse_options options;
		tg3_error_stack errors;
		tg3_model model;

		tg3_parse_options_init(&options);
		tg3_error_stack_init(&errors);

		// Parse
		auto err = tg3_parse_file(&model, &errors, path.data(), static_cast<uint32_t>(path.size()), &options);
		if (err != TG3_OK)
		{
			Log::Error("Failed loading mesh '{}'", path);
			for (uint32_t i = 0; i < errors.count; i++)
				Log::Error("{}", errors.entries[i].message ? errors.entries[i].message : "NULL");
		}

		ProcessMaterials(model);

		auto pos = path.find_last_of("\\/");
		m_Name = pos == std::string_view::npos ? path : path.substr(pos + 1);
		ProcessImages(model, pos == std::string_view::npos ? "" : path.substr(0, pos + 1));

		for (uint32_t i = 0; i < model.nodes_count; i++)
		{
			const auto& node = model.nodes[i];
			ProcessNode(model, node);
		}

		// Free
		tg3_model_free(&model);
		tg3_error_stack_free(&errors);
	}

	auto Mesh::CreateMeshPrimitive(const MeshPrimitiveType type) -> Ref<Mesh>
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

		const auto vb = VertexBuffer::CreateMeshPrimitive(type);
		const auto ib = IndexBuffer::CreateMeshPrimitive(type);

		Ref<Material> material = CreateRef<Material>();

		Primitive primitive{
			.VertexCount = vb->GetSize() / sizeof(Vertex),
			.IndexCount = ib->GetIndexCount(),
			.Material = material,
		};

		Submesh submesh{
			.Name = "Cube",
			.VertexBuffer = vb,
			.IndexBuffer = ib,
			.Primitives = { primitive },
			.LocalTransform = glm::mat4(1.0f),
		};

		Ref<Mesh> mesh = CreateRef<Mesh>();
		mesh->m_Submeshes.emplace_back(submesh);
		mesh->m_Materials.emplace_back(material);
		mesh->m_Name = MeshPrimitiveTypeToString(type);

		return mesh;
	}

	auto Mesh::ProcessNode(const tg3_model& model, const tg3_node& node) -> void
	{
		EP_PROFILE_FN("Mesh::ProcessNode")

		if (const int32_t meshIndex = node.mesh; meshIndex > -1)
		{
			auto localTransform = glm::mat4(1.0f);

			const glm::vec3 translation = glm::make_vec3(node.translation);
			localTransform = glm::translate(localTransform, translation);

			const glm::quat rotation = glm::make_quat(node.rotation);
			localTransform *= glm::mat4(rotation);

			const glm::vec3 scale = glm::make_vec3(node.scale);
			localTransform = glm::scale(localTransform, scale);

			ProcessMesh(model, model.meshes[meshIndex], localTransform);
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

			for (uint32_t j = 0; j < mp.attributes_count; j++)
			{
				const auto& attribute = mp.attributes[j];
				if (std::strncmp(attribute.key.data, "POSITION", 8) == 0)
				{
					EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
					const auto& accessor = model.accessors[attribute.value];
					const auto& bufferView = model.buffer_views[accessor.buffer_view];
					positionData = reinterpret_cast<const float*>(&model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]);
					vertexCount = accessor.count;
				}

				if (std::strncmp(attribute.key.data, "NORMAL", 6) == 0)
				{
					EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
					const auto& accessor = model.accessors[attribute.value];
					const auto& bufferView = model.buffer_views[accessor.buffer_view];
					normalData = reinterpret_cast<const float*>(&model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]);
				}

				if (std::strncmp(attribute.key.data, "TEXCOORD_0", 10) == 0)
				{
					EP_ASSERT(attribute.value <= static_cast<int32_t>(model.accessors_count) && attribute.value > -1);
					const auto& accessor = model.accessors[attribute.value];
					const auto& bufferView = model.buffer_views[accessor.buffer_view];
					texCoordData = reinterpret_cast<const float*>(&model.buffers[bufferView.buffer].data.data[accessor.byte_offset + bufferView.byte_offset]);
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

			if (mp.material != -1)
				p.Material = m_Materials.at(mp.material);
		}
		
		data.VertexBuffer = CreateRef<VertexBuffer>(vertices.data(), static_cast<uint64_t>(vertices.size() * sizeof(Vertex)));
		data.IndexBuffer = CreateRef<IndexBuffer>(indices.data(), static_cast<uint64_t>(indices.size() * sizeof(uint32_t)));
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

			if (material.pbr_metallic_roughness.base_color_texture.index != -1)
			{
				const auto index = material.pbr_metallic_roughness.base_color_texture.index;
				newMat->DiffuseMapIndex = model.textures[index].source;
			}

			if (material.normal_texture.index != -1)
			{
				const auto index = material.normal_texture.index;
				newMat->NormalMapIndex = model.textures[index].source;
			}

			if (material.pbr_metallic_roughness.metallic_roughness_texture.index != -1)
			{
				const auto index = material.pbr_metallic_roughness.metallic_roughness_texture.index;
				newMat->RoughMetMapIndex = model.textures[index].source;
			}

			m_Materials[i] = newMat;
		}
	}

	auto Mesh::ProcessImages(const tg3_model& model, std::string_view basePath) -> void
	{
		EP_PROFILE_FN("Mesh::ProcessImages")

		const auto device = DeviceManager::Get()->GetDevice();

		// Format look up
		std::unordered_map<uint32_t, nvrhi::Format> imageFormats;
		for (const auto& mat : m_Materials)
		{
			if (mat->DiffuseMapIndex >= 0)
				imageFormats[mat->DiffuseMapIndex] = nvrhi::Format::SRGBA8_UNORM;
			if (mat->NormalMapIndex >= 0)
				imageFormats[mat->NormalMapIndex] = nvrhi::Format::RGBA8_UNORM;
			if (mat->RoughMetMapIndex >= 0)
				imageFormats[mat->RoughMetMapIndex] = nvrhi::Format::RGBA8_UNORM;
		}

		m_Images.resize(model.images_count);
		std::vector<nvrhi::CommandListHandle> cmdLists(m_Images.size());

		std::for_each(std::execution::par, m_Images.begin(), m_Images.end(), [&, model](auto& img)
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

					std::string name = std::string(bufferView.name.data, bufferView.name.len);

					nvrhi::Format format = nvrhi::Format::UNKNOWN;
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
		});

		// We need the raw pointers, but because of the only strong reference, stored it in a vector as a shared ptr
		std::vector<nvrhi::ICommandList*> rawCmds;
		rawCmds.reserve(cmdLists.size());
		for (const auto& cmd : cmdLists)
			rawCmds.emplace_back(cmd.Get());

		device->executeCommandLists(rawCmds.data(), cmdLists.size());
	}
}