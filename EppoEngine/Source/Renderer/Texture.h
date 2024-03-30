#pragma once

#include "Asset/Asset.h"

typedef unsigned int GLenum;

namespace Eppo
{
	enum class TextureWrap
	{
		CLAMP_TO_EDGE,
		CLAMP_TO_BORDER,
		MIRRORED_REPEAT,
		REPEAT,
		MIRROR_CLAMP_TO_EDGE
	};

	enum class TextureMinFilter
	{
		NEAREST,
		LINEAR,
		NEAREST_MIPMAP_NEAREST,
		LINEAR_MIPMAP_NEAREST,
		NEAREST_MIPMAP_LINEAR,
		LINEAR_MIPMAP_LINEAR
	};

	enum class TextureMaxFilter
	{
		NEAREST,
		LINEAR
	};

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

		TextureMinFilter MinFilter = TextureMinFilter::LINEAR;
		TextureMaxFilter MaxFilter = TextureMaxFilter::LINEAR;
		TextureWrap Wrap = TextureWrap::REPEAT;
		glm::vec4 BorderColor = glm::vec4(0.0f);

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
