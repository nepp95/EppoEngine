#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	class ShaderLibrary
	{
	public:
		ShaderLibrary() = default;
		~ShaderLibrary() = default;
		ShaderLibrary(ShaderLibrary&) = delete;
		ShaderLibrary& operator=(const ShaderLibrary&) = delete;

		void Load(std::string_view path);
		const Ref<Shader>& Get(const std::string& name);

	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}
