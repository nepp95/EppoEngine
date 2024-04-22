#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord = { 0.0f, 0.0f };
	};
}
