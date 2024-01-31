#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
    class OpenGLShader : public Shader
    {
    public:
        OpenGLShader(const ShaderSpecification& specification);
        ~OpenGLShader();

        const std::string& GetName() const override { return m_Name; }

    private:
        ShaderSpecification m_Specification;
        std::string m_Name;
    };
}
