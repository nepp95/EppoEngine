#pragma once

#include "Asset/Asset.h"

typedef unsigned int GLenum;

namespace Eppo
{
	enum class TextureFormat
	{
		RGB,
		RGBA,

		Depth
	};

	struct TextureSpecification
	{
		std::filesystem::path Filepath;

		TextureFormat Format;

		uint32_t Width;
		uint32_t Height;
	};

	class Texture : public Asset
	{
	public:
		Texture(const TextureSpecification& specification);
		~Texture();

		void RT_Bind() const;

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

		uint32_t GetRendererID() const { return m_RendererID; }

		// Asset
		static AssetType GetStaticType() { return AssetType::Texture; }

	private:
		void SetupParameters() const;

	private:
		uint32_t m_RendererID;
		TextureSpecification m_Specification;
	};
}
