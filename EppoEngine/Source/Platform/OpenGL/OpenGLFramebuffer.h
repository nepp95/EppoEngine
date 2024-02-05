#pragma once

#include "Renderer/Framebuffer.h"

namespace Eppo
{
	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& specification);
		~OpenGLFramebuffer();

		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }

	private:
		FramebufferSpecification m_Specification;

		uint32_t m_RendererID;
	};
}
