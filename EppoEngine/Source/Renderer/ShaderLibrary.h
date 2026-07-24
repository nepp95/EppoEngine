#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	class ShaderLibrary
	{
	public:
		ShaderLibrary() = default;
		~ShaderLibrary() = default;

		auto Load(const std::string& name, const std::unordered_map<nvrhi::ShaderType, std::string>& sources = {}) -> void;
		[[nodiscard]] auto Get(const std::string& name) const -> Ref<Shader>;
	    [[nodiscard]] auto GetAll() const -> const std::unordered_map<std::string, Ref<Shader>>& { return m_Shaders; }

	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}
