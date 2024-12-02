#pragma once

#include "Renderer/Mesh/Mesh.h"
#include "Renderer/RenderTypes.h"
#include "Scene/Entity.h"

#include <glm/glm.hpp>

namespace Eppo
{
	struct DrawCommand
	{
		EntityHandle Handle;
	};

	struct MeshCommand : public DrawCommand
	{
		Ref<Mesh> Mesh;
		glm::mat4 Transform;
	};

	struct PointLightCommand : public DrawCommand
	{
		glm::vec4 Color;
		glm::vec3 Position;
	};
}
