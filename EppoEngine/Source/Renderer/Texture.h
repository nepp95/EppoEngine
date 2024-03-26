#pragma once

#include "Asset/Asset.h"

typedef unsigned int GLenum;

namespace Eppo
{
	class Texture : public Asset
	{
	public:
		Texture(const std::filesystem::path& filepath);
		Texture(uint32_t width, uint32_t height);
		~Texture();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		uint32_t GetRendererID() const { return m_RendererID; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Texture; }

	private:
		std::filesystem::path m_Filepath;
		uint32_t m_RendererID;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		GLenum m_InternalFormat;
		GLenum m_DataFormat;
	};
}
