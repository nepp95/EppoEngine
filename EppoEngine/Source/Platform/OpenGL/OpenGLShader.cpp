#include "pch.h"
#include "OpenGLShader.h"

namespace Eppo
{
    OpenGLShader::OpenGLShader(const ShaderSpecification& specification)
        : m_Specification(specification)
    {}

    OpenGLShader::~OpenGLShader()
    {}
}
