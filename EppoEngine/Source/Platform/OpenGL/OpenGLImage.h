#pragma once

#include "Renderer/Image.h"

#include <glad/glad.h>

namespace Eppo
{
	class OpenGLImage : public Image
	{
	public:
		OpenGLImage(const ImageSpecification& specification);
		~OpenGLImage();

		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }

	private:
		ImageSpecification m_Specification;

		uint32_t m_RendererID;
	};

	namespace Utils
	{
		inline GLenum ImageFormatToGLFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::None:			return GL_INVALID_ENUM;
				case ImageFormat::RGBA8:		return GL_RGBA8;
				case ImageFormat::RGBA8_UNORM:	return GL_RGBA8_SNORM;
				case ImageFormat::Depth:		return GL_DEPTH24_STENCIL8;
			}

			EPPO_ASSERT(false);
			return GL_INVALID_ENUM;
		}

		inline ImageFormat GLFormatToImageFormat(GLenum format)
		{
			switch (format)
			{
				case GL_INVALID_ENUM:			return ImageFormat::None;
				case GL_RGBA8:					return ImageFormat::RGBA8;
				case GL_RGBA8_SNORM:			return ImageFormat::RGBA8_UNORM;
				case GL_DEPTH24_STENCIL8:		return ImageFormat::Depth;
			}

			EPPO_ASSERT(false);
			return ImageFormat::None;
		}
	}
}
