#include "pch.h"
#include "Texture.h"

#include "Renderer/Renderer.h"
#include "Renderer/RendererContext.h"

#include <glad/glad.h>
#include <stb_image.h>

namespace Eppo
{
	Texture::Texture(const std::filesystem::path& filepath)
		: m_Filepath(filepath)
	{
		EPPO_PROFILE_FUNCTION("Texture::Texture");

		// Read pixels
		int width, height, channels;
		stbi_uc* data = nullptr;

		stbi_set_flip_vertically_on_load(1);

		data = stbi_load(filepath.string().c_str(), &width, &height, &channels, 0);

		if (data)
		{
			m_Width = width;
			m_Height = height;

			if (channels == 4)
			{
				m_InternalFormat = GL_RGBA8;
				m_DataFormat = GL_RGBA;
			} else if (channels == 3)
			{
				m_InternalFormat = GL_RGB8;
				m_DataFormat = GL_RGB;
			}

			EPPO_ASSERT(m_InternalFormat & m_DataFormat);

			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
			glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);
		}
	}

	Texture::Texture(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		EPPO_PROFILE_FUNCTION("Texture::Texture");

		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	Texture::~Texture()
	{
		EPPO_PROFILE_FUNCTION("Texture::~Texture");

		glDeleteTextures(1, &m_RendererID);
	}
}
