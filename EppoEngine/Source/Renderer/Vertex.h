#pragma once

#include "Vulkan.h"

#include <glm/glm.hpp>

#include <array>

namespace Eppo
{
	struct Vertex
	{
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::vec4 Color = { 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec2 TexCoord = { 0.0f, 0.0f };
		float TexIndex = 0.0f;
	};

	struct MeshVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord = { 0.0f, 0.0f };

		static VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(MeshVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(MeshVertex, Position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(MeshVertex, Normal);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(MeshVertex, TexCoord);

			return attributeDescriptions;
		}
	};
}
