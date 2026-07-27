#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
	class VulkanShader : public Shader
	{
	public:
        explicit VulkanShader(ShaderSpecification spec);

	private:
		auto CompileOrGetCache() -> bool;
		auto Compile(nvrhi::ShaderType type) -> bool;
		auto Reflect(nvrhi::ShaderType type) -> void;
	};
}