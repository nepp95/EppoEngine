#pragma once

namespace Eppo
{
	struct Material
	{
		float Roughness = 0.0f;
		float Metallic = 0.0f;
		float NormalMapIntensity = 0.0f;
		glm::vec4 DiffuseColor = glm::vec4(1.0f);

		int32_t DiffuseMapIndex = -1;
		int32_t NormalMapIndex = -1;
		int32_t RoughnessMetallicMapIndex = -1;
	};
}
