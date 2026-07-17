#pragma once

#include "Renderer/ShaderLibrary.h"

namespace Eppo
{
    class DescriptorManager;

	class Renderer
	{
	public:
		Renderer();

		[[nodiscard]] auto GetShader(const std::string& name) const -> Ref<Shader>;
	    [[nodiscard]] auto GetDescriptorManager() const -> const Ref<DescriptorManager>&;

	private:
		ShaderLibrary m_ShaderLibrary;
	    Ref<DescriptorManager> m_DescriptorManager = nullptr;
	};
}