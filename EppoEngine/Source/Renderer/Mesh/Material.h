#pragma once

namespace Eppo
{
	enum MaterialFeatures
	{
		DiffuseMap = 1 << 0,
		NormalMap = 1 << 1,
		RoughnessMetallicMap = 1 << 2
	};

	struct Material
	{
		uint32_t FeatureFlags = 0;

		float Roughness = 0.0f;
		float Metallic = 0.0f;
		float NormalMapIntensity = 0.0f;
		glm::vec4 DiffuseColor = glm::vec4(1.0f);

		uint32_t DiffuseMapIndex = 0;
		uint32_t NormalMapIndex = 0;
		uint32_t RoughnessMetallicMapIndex = 0;

		bool HasFeature(MaterialFeatures feature) const
		{
			return FeatureFlags & feature;
		}
	};
}
