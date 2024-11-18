#include "pch.h"
#include "Mesh.h"

#include "Renderer/Vertex.h"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>
#include <tiny_gltf.h>

namespace Eppo
{
	Mesh::Mesh(const std::filesystem::path& filepath)
		: m_Filepath(filepath)
	{
		EPPO_PROFILE_FUNCTION("Mesh::Mesh");

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string error;
		std::string warning;

		bool result = loader.LoadBinaryFromFile(&model, &error, &warning, m_Filepath.string());

		if (!warning.empty())
			EPPO_WARN(warning);
		if (!error.empty())
			EPPO_ERROR(error);

		if (!result)
		{
			EPPO_ERROR("Failed to parse mesh file '{}'!", m_Filepath.string());
			return;
		}

		ProcessMaterials(model);
		ProcessImages(model);

		for (const auto& node : model.nodes)
			ProcessNode(model, node);
	}

	void Mesh::ProcessNode(const tinygltf::Model& model, const tinygltf::Node& node)
	{
		int32_t meshIndex = node.mesh;

		if (meshIndex > -1)
		{
			// Get local mesh transform
			glm::mat4 localTransform = glm::mat4(1.0f);

			if (!node.translation.empty())
			{
				glm::vec3 translation = glm::make_vec3(node.translation.data());
				localTransform = glm::translate(localTransform, translation);
			}

			if (!node.rotation.empty())
			{
				glm::quat rotation = glm::make_quat(node.rotation.data());
				localTransform *= glm::mat4(rotation);
			}

			if (!node.scale.empty())
			{
				glm::vec3 scale = glm::make_vec3(node.scale.data());
				localTransform = glm::scale(localTransform, scale);
			}

			MeshData meshData = GetVertexData(model, model.meshes[meshIndex]);
			Submesh& submesh = m_Submeshes.emplace_back(node.name, meshData.Vertices, meshData.Indices, meshData.Primitives, localTransform);
		}

		for (size_t i = 0; i < node.children.size(); i++)
			ProcessNode(model, model.nodes[i]);
	}

	void Mesh::ProcessMaterials(const tinygltf::Model& model)
	{
		m_Materials.resize(model.materials.size());
		for (size_t i = 0; i < model.materials.size(); i++)
		{
			const tinygltf::Material& mat = model.materials[i];

			Ref<Material> material = CreateRef<Material>();
			material->Roughness = mat.pbrMetallicRoughness.roughnessFactor;
			material->Metallic = mat.pbrMetallicRoughness.metallicFactor;
			material->NormalMapIntensity = mat.normalTexture.scale;

			// Diffuse color
			if (mat.values.find("baseColorFactor") != mat.values.end())
				material->DiffuseColor = glm::make_vec4(mat.values.at("baseColorFactor").ColorFactor().data());

			// Diffuse texture
			if (mat.pbrMetallicRoughness.baseColorTexture.index != -1)
				material->DiffuseMapIndex = model.textures[mat.pbrMetallicRoughness.baseColorTexture.index].source;
			
			// Normal texture
			if (mat.normalTexture.index != -1)
				material->NormalMapIndex = model.textures[mat.normalTexture.index].source;

			// Roughness/Metallic texture
			if (mat.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
				material->RoughnessMetallicMapIndex = model.textures[mat.pbrMetallicRoughness.metallicRoughnessTexture.index].source;

			m_Materials[i] = material;
		}
	}

	void Mesh::ProcessImages(const tinygltf::Model& model)
	{
		for (const auto& image : model.images)
		{
			ImageSpecification imageSpec;
			imageSpec.Width = image.width;
			imageSpec.Height = image.height;
			imageSpec.Usage = ImageUsage::Texture;

			if (image.component == 4)
			{
				imageSpec.Format = ImageFormat::RGBA8;
				CreateImage(imageSpec, (void*)image.image.data(), 4);
			}
			else
			{
				EPPO_ASSERT(false); // TODO: not supported yet
			}
		}
	}

	MeshData Mesh::GetVertexData(const tinygltf::Model& model, const tinygltf::Mesh& mesh)
	{
		MeshData meshData;

		for (const auto& primitive : mesh.primitives)
		{
			Primitive& p = meshData.Primitives.emplace_back();
			p.FirstVertex = static_cast<uint32_t>(meshData.Vertices.size());
			p.FirstIndex = static_cast<uint32_t>(meshData.Indices.size());

			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;

			const float* positionData = nullptr;
			const float* normalData = nullptr;
			const float* texCoordData = nullptr;

			// Vertices
			if (primitive.attributes.find("POSITION") != primitive.attributes.end())
			{
				const auto& accessor = model.accessors[primitive.attributes.find("POSITION")->second];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				positionData = reinterpret_cast<const float*>(&(model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]));
				vertexCount = accessor.count;
			}

			if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
			{
				const auto& accessor = model.accessors[primitive.attributes.find("NORMAL")->second];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				normalData = reinterpret_cast<const float*>(&(model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]));
			}

			if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
			{
				const auto& accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				texCoordData = reinterpret_cast<const float*>(&(model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]));
			}

			size_t offset = meshData.Vertices.size();
			meshData.Vertices.resize(offset + vertexCount);

			for (size_t i = 0; i < vertexCount; i++)
			{
				Vertex& vertex = meshData.Vertices[offset + i];
				vertex.Position = glm::make_vec3(&positionData[i * 3]);
				vertex.Normal = glm::make_vec3(&normalData[i * 3]);
				vertex.TexCoord = glm::make_vec2(&texCoordData[i * 2]);
				vertex.Color = glm::vec4(vertex.Normal, 1.0f);
			}

			// Indices
			const auto& accessor = model.accessors[primitive.indices];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			const auto& buffer = model.buffers[bufferView.buffer];

			meshData.Indices.reserve(accessor.count + meshData.Indices.size());

			if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			{
				const auto* data = reinterpret_cast<const uint32_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
				for (size_t i = 0; i < accessor.count; i++)
					meshData.Indices.push_back(data[i]);
			}
			else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				const auto* data = reinterpret_cast<const uint16_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
				for (size_t i = 0; i < accessor.count; i++)
					meshData.Indices.push_back(data[i]);
			}
			else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				const auto* data = reinterpret_cast<const uint8_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
				for (size_t i = 0; i < accessor.count; i++)
					meshData.Indices.push_back(data[i]);
			}

			p.VertexCount = vertexCount;
			p.IndexCount = accessor.count;

			// Material
			if (primitive.material != -1)
				p.Material = m_Materials[primitive.material];
		}

		return meshData;
	}

	void Mesh::CreateImage(const ImageSpecification& imageSpec, void* data, uint32_t channels)
	{
		Ref<Image> image = m_Images.emplace_back(CreateRef<Image>(imageSpec));

		uint64_t size = imageSpec.Width * imageSpec.Height * channels;

		// Create staging buffer
		VkBufferCreateInfo stagingBufferInfo{};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.size = size;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAlloc = Allocator::AllocateBuffer(stagingBuffer, stagingBufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);

		void* memData = Allocator::MapMemory(stagingBufferAlloc);
		memcpy(memData, data, size);
		Allocator::UnmapMemory(stagingBufferAlloc);

		VkCommandBuffer commandBuffer = RendererContext::Get()->GetLogicalDevice()->GetCommandBuffer(true);

		// Transition to layout optimal for transferring
		Image::TransitionImage(commandBuffer, image->GetImageInfo().Image, image->GetImageInfo().ImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy image data to image
		VkBufferImageCopy copyRegion{};
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageExtent.width = imageSpec.Width;
		copyRegion.imageExtent.height = imageSpec.Height;
		copyRegion.imageExtent.depth = 1;
		copyRegion.bufferOffset = 0;

		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Transition image back to presentable layout
		Image::TransitionImage(commandBuffer, image->GetImageInfo().Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Flush command buffer
		RendererContext::Get()->GetLogicalDevice()->FlushCommandBuffer(commandBuffer);

		Allocator::DestroyBuffer(stagingBuffer, stagingBufferAlloc);
	}
}
