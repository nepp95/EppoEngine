#pragma once

#include "Renderer/Framebuffer.h"

namespace Eppo
{
	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& specification);
		virtual ~OpenGLFramebuffer();

		void Resize(uint32_t width, uint32_t height) override;

		const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		Ref<Image> GetFinalImage() const override;
		bool HasDepthAttachment() const override;
		Ref<Image> GetDepthImage() const override;

		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }

	private:
		FramebufferSpecification m_Specification;

		uint32_t m_RendererID;
	};
}
