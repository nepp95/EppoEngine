#pragma once

#include "Asset/Asset.h"

namespace Eppo
{
	class Material : public Asset
	{
	public:
		Material() = default;

		std::string m_Name;

		glm::vec3 m_AmbientColor;
		glm::vec3 m_DiffuseColor;
		glm::vec3 m_SpecularColor;

		float Roughness;
	};
}
