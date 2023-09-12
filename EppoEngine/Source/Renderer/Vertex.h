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
	};
}
