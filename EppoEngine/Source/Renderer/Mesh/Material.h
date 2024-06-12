#pragma once

#include "Asset/Asset.h"
#include "Renderer/Texture.h"

namespace Eppo
{
	struct Material : public Asset
	{
		std::string Name;

		glm::vec3 AmbientColor = glm::vec3(0.0f);
		glm::vec3 DiffuseColor = glm::vec3(0.0f);
		glm::vec3 SpecularColor = glm::vec3(0.0f);

		float Roughness = 0.0f;

		Ref<Texture> DiffuseTexture = nullptr;
		Ref<Texture> NormalTexture = nullptr;
	};
}
