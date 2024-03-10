#include "pch.h"
#include "OpenGLImage.h"

#include <glad/glad.h>

namespace Eppo
{
	OpenGLImage::OpenGLImage(const ImageSpecification& specification)
		: m_Specification(specification)
	{
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(
			GL_TEXTURE_2D,
			1,
			Utils::ImageFormatToGLFormat(m_Specification.Format),
			m_Specification.Width,
			m_Specification.Height
		);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	OpenGLImage::~OpenGLImage()
	{
		glDeleteTextures(1, &m_RendererID);
	}
}
