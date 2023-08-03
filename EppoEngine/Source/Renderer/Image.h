#pragma once

#include "Renderer/Allocator.h"

namespace Eppo
{
	enum class ImageFormat
	{
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

	class Image
	{
	public:
		Image(const ImageSpecification& specification);
		~Image();

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

	private:
		ImageSpecification m_Specification;

		VkImage m_Image;
		VkImageView m_ImageView;
		VkImageLayout m_ImageLayout;

		VmaAllocation m_Allocation;
	};
}
