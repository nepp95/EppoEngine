#pragma once

#include <glm/glm.hpp>

namespace Eppo
{
	enum class EntityType : uint8_t
	{
		Mesh,
		PointLight
	};

	struct PointLight
	{
		glm::mat4 View[6];
		glm::vec4 Position = glm::vec4(0.0f);
		glm::vec4 Color = glm::vec4(0.0f);
	};
}
