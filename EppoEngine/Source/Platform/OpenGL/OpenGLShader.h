#pragma once

#include "Renderer/Shader.h"

namespace Eppo
{
    class OpenGLShader : public Shader
    {
    public:
        OpenGLShader(const ShaderSpecification& specification);
        ~OpenGLShader();

		void Bind() const;
		void Unbind() const;

	protected:
		void Reflect() override;

	private:
		void Link();

    private:
		uint32_t m_RendererID;

		std::unordered_map<uint32_t, std::vector<ShaderResource>> m_ShaderResources;
	};
}
