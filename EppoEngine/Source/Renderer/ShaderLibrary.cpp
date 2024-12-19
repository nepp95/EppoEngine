#include "pch.h"
#include "ShaderLibrary.h"

namespace Eppo
{
	void ShaderLibrary::Load(const std::string_view path)
	{
		EPPO_PROFILE_FUNCTION("ShaderLibrary::Load");

		ShaderSpecification spec;
		spec.Filepath = path;

		const Ref<Shader> shader = Shader::Create(spec);
		const std::string& name = shader->GetName();

		std::scoped_lock lock(m_Mutex);

		m_Shaders[name] = shader;
	}

	const Ref<Shader>& ShaderLibrary::Get(const std::string& name)
	{
		EPPO_PROFILE_FUNCTION("ShaderLibrary::Get");

		EPPO_ASSERT(m_Shaders.find(name) != m_Shaders.end())
		return m_Shaders.at(name);
	}
}
