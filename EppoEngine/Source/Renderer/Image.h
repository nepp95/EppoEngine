#pragma once

#include "Renderer/RendererContext.h"

namespace Eppo
{
	enum class ImageFormat
	{
		None = 0,

		// Color
		RGBA8,
		RGBA8_UNORM,

		// Depth
		Depth
	};

	enum class ImageUsage
	{
		Texture,
		Attachment
	};

	struct ImageSpecification
	{
		uint32_t Width;
		uint32_t Height;

		ImageFormat Format;
		ImageUsage Usage;
	};

	class Image : public RefCounter
	{
	public:
		virtual ~Image() {};

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		static Ref<Image> Create(const ImageSpecification& specification);
	};

	namespace Utils
	{
		inline uint32_t GetMemorySize(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::RGBA8:		return 4;
				case ImageFormat::RGBA8_UNORM:	return 4;
			}

			EPPO_ASSERT(false);
			return 0;
		}

		inline bool IsDepthFormat(ImageFormat format)
		{
			if (format == ImageFormat::Depth)
				return true;
			return false;
		}
	}
}
