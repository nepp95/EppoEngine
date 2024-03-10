#include "pch.h"
#include "ShaderLibrary.h"

namespace Eppo
{
	void ShaderLibrary::Load(std::string_view path)
	{
		ShaderSpecification spec;
		spec.Filepath = path;
		spec.Optimize = false;

		Ref<Shader> shader = Shader::Create(spec);
		const std::string& name = shader->GetName();

		m_Shaders[name] = shader;
	}

	const Ref<Shader>& ShaderLibrary::Get(const std::string& name)
	{
		EPPO_ASSERT(m_Shaders.find(name) != m_Shaders.end());
		return m_Shaders.at(name);
	}
}
