#include "pch.h"
#include "Renderer/Renderer.h"

namespace Eppo
{
	Renderer::Renderer()
	{
		m_ShaderLibrary.Load("geometry");
		m_ShaderLibrary.Load("skybox");
		m_ShaderLibrary.Load("imgui");
		m_ShaderLibrary.Load("wireframe");
	}

	auto Renderer::GetShader(const std::string& name) const -> Ref<Shader>
	{
		return m_ShaderLibrary.Get(name);
	}
}