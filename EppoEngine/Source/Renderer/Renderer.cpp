#include "pch.h"
#include "Renderer/Renderer.h"

#include "Renderer/DescriptorManager.h"

namespace Eppo
{
	Renderer::Renderer()
	{
	    m_DescriptorManager = CreateRef<DescriptorManager>();

		m_ShaderLibrary.Load("geometry");
		m_ShaderLibrary.Load("skybox");
		m_ShaderLibrary.Load("imgui");
		m_ShaderLibrary.Load("wireframe");
	}

	auto Renderer::GetShader(const std::string& name) const -> Ref<Shader>
	{
		return m_ShaderLibrary.Get(name);
	}

    auto Renderer::GetDescriptorManager() const -> const Ref<DescriptorManager>&
	{
	    return m_DescriptorManager;
	}
}
