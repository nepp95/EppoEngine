#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
		glm::vec4 Color;

		Vertex() = default;
		Vertex(const glm::vec3& position, const glm::vec4& color)
			: Position(position), Color(color)
		{}
	};
}
