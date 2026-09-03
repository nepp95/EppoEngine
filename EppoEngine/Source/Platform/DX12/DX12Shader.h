#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
    class DX12Shader : public Shader
    {
    public:
        explicit DX12Shader(ShaderSpecification spec);

    private:
        auto Reflect(nvrhi::ShaderType type) -> bool;
    };
}
