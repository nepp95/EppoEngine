#pragma once

#include "Renderer/Texture.h"

namespace Eppo
{
	class OpenGLTexture : public Texture
	{
	public:
		OpenGLTexture(const std::filesystem::path& filepath);
		OpenGLTexture(uint32_t width, uint32_t height, ImageFormat format, void* data);
		virtual ~OpenGLTexture();

		uint32_t GetRendererID() const { return m_RendererID; }

	private:
		uint32_t m_RendererID;
	};
}
