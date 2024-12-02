#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	struct Vertex
	{
		glm::vec3 Position	= glm::vec3(0.0f);
		glm::vec3 Normal	= glm::vec3(0.0f);
		glm::vec2 TexCoord	= glm::vec2(0.0f);
		glm::vec4 Color		= glm::vec4(0.0f);

		Vertex() = default;
		Vertex(const glm::vec3& position, const glm::vec4& color)
			: Position(position), Color(color)
		{}
	};

	struct LineVertex
	{
		glm::vec3 Position	= glm::vec3(0.0f);
		glm::vec4 Color		= glm::vec4(0.0f);

		LineVertex() = default;
		LineVertex(const glm::vec3& position, const glm::vec4& color)
			: Position(position), Color(color)
		{}
	};
}
