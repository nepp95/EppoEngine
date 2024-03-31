#include "pch.h"
#include "Texture.h"

#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace Eppo
{
	namespace Utils
	{
		static GLenum TextureFormatToGLInternalFormat(TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGB:	return GL_RGB8;
				case TextureFormat::RGBA:	return GL_RGBA8;
				case TextureFormat::Depth:	return GL_DEPTH_COMPONENT;
			}

			EPPO_ASSERT(false);
			return 0;
		}

		static GLenum TextureFormatToGLDataFormat(TextureFormat format)
		{
			switch (format)
			{
				case TextureFormat::RGB:	return GL_RGB;
				case TextureFormat::RGBA:	return GL_RGBA;
				case TextureFormat::Depth:	return GL_DEPTH24_STENCIL8;
			}

			EPPO_ASSERT(false);
			return 0;
		}

		static GLenum TextureWrapToGLTextureWrap(TextureWrap wrap)
		{
			switch (wrap)
			{
				case TextureWrap::CLAMP_TO_EDGE:		return GL_CLAMP_TO_EDGE;
				case TextureWrap::CLAMP_TO_BORDER:		return GL_CLAMP_TO_BORDER;
				case TextureWrap::MIRRORED_REPEAT:		return GL_MIRRORED_REPEAT;
				case TextureWrap::REPEAT:				return GL_REPEAT;
				case TextureWrap::MIRROR_CLAMP_TO_EDGE:	return GL_MIRROR_CLAMP_TO_EDGE;
			}

			EPPO_ASSERT(false);
			return 0;
		}

		static GLenum TextureMinFilterToGLMinFilter(TextureMinFilter filter)
		{
			switch (filter)
			{
				case TextureMinFilter::NEAREST:					return GL_NEAREST;
				case TextureMinFilter::LINEAR:					return GL_LINEAR;
				case TextureMinFilter::NEAREST_MIPMAP_NEAREST:	return GL_NEAREST_MIPMAP_NEAREST;
				case TextureMinFilter::LINEAR_MIPMAP_NEAREST:	return GL_LINEAR_MIPMAP_NEAREST;
				case TextureMinFilter::NEAREST_MIPMAP_LINEAR:	return GL_NEAREST_MIPMAP_LINEAR;
				case TextureMinFilter::LINEAR_MIPMAP_LINEAR:	return GL_LINEAR_MIPMAP_LINEAR;
			}

			EPPO_ASSERT(false);
			return 0;
		}

		static GLenum TextureMaxFilterToGLMaxFilter(TextureMaxFilter filter)
		{
			switch (filter)
			{
				case TextureMaxFilter::NEAREST:	return GL_NEAREST;
				case TextureMaxFilter::LINEAR:	return GL_LINEAR;
			}

			EPPO_ASSERT(false);
			return 0;
		}
	}

	Texture::Texture(const TextureSpecification& specification)
		: m_Specification(specification)
	{
		EPPO_PROFILE_FUNCTION("Texture::Texture");

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);

		if (!m_Specification.Filepath.empty())
		{
			// Read pixels
			int width, height, channels;
			stbi_uc* data = nullptr;

			stbi_set_flip_vertically_on_load(1);

			data = stbi_load(m_Specification.Filepath.string().c_str(), &width, &height, &channels, 0);

			if (data)
			{
				m_Specification.Width = width;
				m_Specification.Height = height;

				if (channels == 4)
					m_Specification.Format = TextureFormat::RGBA;
				else if (channels == 3)
					m_Specification.Format = TextureFormat::RGB;

				glTextureStorage2D(m_RendererID, 1, Utils::TextureFormatToGLInternalFormat(m_Specification.Format), width, height);
				SetupParameters();
				glTextureSubImage2D(m_RendererID, 0, 0, 0, width, height, Utils::TextureFormatToGLDataFormat(m_Specification.Format), GL_UNSIGNED_BYTE, data);
				
				stbi_image_free(data);
			}
		} else
		{
			glBindTexture(GL_TEXTURE_2D, m_RendererID);
			glTexStorage2D(GL_TEXTURE_2D, 1, Utils::TextureFormatToGLDataFormat(m_Specification.Format), m_Specification.Width, m_Specification.Height);
			//glTextureStorage2D(m_RendererID, 1, Utils::TextureFormatToGLInternalFormat(m_Specification.Format), m_Specification.Width, m_Specification.Height);
			SetupParameters();
		}
	}

	Texture::~Texture()
	{
		EPPO_PROFILE_FUNCTION("Texture::~Texture");

		glDeleteTextures(1, &m_RendererID);
	}

	void Texture::RT_Bind() const
	{
		Renderer::SubmitCommand([this]()
		{
			glBindTexture(GL_TEXTURE_2D, m_RendererID);
		});
	}

	void Texture::SetupParameters() const
	{
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, Utils::TextureMinFilterToGLMinFilter(m_Specification.MinFilter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, Utils::TextureMaxFilterToGLMaxFilter(m_Specification.MaxFilter));

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, Utils::TextureWrapToGLTextureWrap(m_Specification.Wrap));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, Utils::TextureWrapToGLTextureWrap(m_Specification.Wrap));

		glTextureParameterfv(m_RendererID, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(m_Specification.BorderColor));
	}
}
