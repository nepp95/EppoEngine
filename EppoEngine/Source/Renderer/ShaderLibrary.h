#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	class ShaderLibrary
	{
	public:
		ShaderLibrary() = default;
		~ShaderLibrary() = default;

		auto Load(const std::string& name) -> void;
		[[nodiscard]] auto Get(const std::string& name) const -> Ref<Shader>;

	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}
