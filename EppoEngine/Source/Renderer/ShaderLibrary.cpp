#include "pch.h"
#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
	auto ShaderLibrary::Load(ShaderSpecification spec) -> void
	{
		const std::string name = spec.Name;
		if (m_Shaders.contains(name))
			Log::Warn("Shader with name '{}' already exists, reloading shader!", name);

		m_Shaders[name] = Shader::Create(std::move(spec));
	}

	auto ShaderLibrary::Get(const std::string& name) const -> Ref<Shader>
	{
		if (m_Shaders.contains(name))
			return m_Shaders.at(name);

		Log::Error("Shader with name '{}' not found!", name);
		return nullptr;
	}
}
