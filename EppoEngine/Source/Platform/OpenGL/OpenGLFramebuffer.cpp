#include "pch.h"
#include "OpenGLFramebuffer.h"

#include "Core/Application.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& specification)
		: m_Specification(specification)
	{
		if (m_Specification.Width == 0)
			m_Specification.Width = Application::Get().GetWindow().GetWidth();

		if (m_Specification.Height == 0)
			m_Specification.Height = Application::Get().GetWindow().GetHeight();

		glCreateFramebuffers(1, &m_RendererID);

		for (const auto& attachment : m_Specification.Attachments)
		{
			ImageSpecification imageSpec;
			imageSpec.Format = attachment;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.Width = GetWidth();
			imageSpec.Height = GetHeight();
		}
	}

	OpenGLFramebuffer::~OpenGLFramebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{

	}

	Ref<Image> OpenGLFramebuffer::GetFinalImage() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	bool OpenGLFramebuffer::HasDepthAttachment() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	Ref<Image> OpenGLFramebuffer::GetDepthImage() const
	{
		throw std::logic_error("The method or operation is not implemented.");
	}
}
