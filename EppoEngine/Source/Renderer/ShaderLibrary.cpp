#include "pch.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	auto ShaderLibrary::Load(const std::string& name) -> void
	{
		if (m_Shaders.contains(name))
			Log::Warn("Shader with name '{}' already exists, reloading shader!", name);
	
		const ShaderSpecification shaderSpec{ .Name = name };

		m_Shaders[name] = Shader::Create(shaderSpec);
	}

	auto ShaderLibrary::Get(const std::string& name) const -> Ref<Shader>
	{
		if (m_Shaders.contains(name))
			return m_Shaders.at(name);

		Log::Error("Shader with name '{}' not found!", name);
		return nullptr;
	}
}
