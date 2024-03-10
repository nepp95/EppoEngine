#pragma once

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

namespace Eppo
{
	class Material : public RefCounter
	{
	public:
		virtual ~Material() {}

		virtual void Set(const std::string& name, Ref<Texture> texture, uint32_t arrayIndex = 0) = 0;

		static Ref<Material> Create(Ref<Shader> shader);
	};
}
