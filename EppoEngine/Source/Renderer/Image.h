#pragma once

#include "Renderer/Vulkan.h"
typedef struct VmaAllocation_T* VmaAllocation;

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

	struct ImageInfo
	{
		VkImage Image;
		VkImageView ImageView;
		VkImageLayout ImageLayout;
		VkSampler Sampler;
		VmaAllocation Allocation;
	};

	class Image
	{
	public:
		Image() = default;
		Image(const ImageSpecification& specification);
		~Image();

		void Release();

		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }

		ImageInfo& GetImageInfo() { return m_Info; }
		VkImageView GetImageView() const { return m_Info.ImageView; }

	private:
		ImageSpecification m_Specification;
		ImageInfo m_Info;
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

		inline VkFormat ImageFormatToVkFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::None:			return VK_FORMAT_UNDEFINED;
				case ImageFormat::RGBA8:		return VK_FORMAT_R8G8B8A8_SRGB;
				case ImageFormat::RGBA8_UNORM:	return VK_FORMAT_R8G8B8A8_UNORM;
				case ImageFormat::Depth:		return VK_FORMAT_D32_SFLOAT;
			}

			EPPO_ASSERT(false);
			return VK_FORMAT_UNDEFINED;
		}

		inline bool IsDepthFormat(ImageFormat format)
		{
			if (format == ImageFormat::Depth)
				return true;
			return false;
		}

		inline ImageFormat VkFormatToImageFormat(VkFormat format)
		{
			switch (format)
			{
				case VK_FORMAT_UNDEFINED:		return ImageFormat::None;
				case VK_FORMAT_R8G8B8A8_SRGB:	return ImageFormat::RGBA8;
				case VK_FORMAT_R8G8B8A8_UNORM:	return ImageFormat::RGBA8_UNORM;
				case VK_FORMAT_D32_SFLOAT:		return ImageFormat::Depth;
			}

			EPPO_ASSERT(false);
			return ImageFormat::None;
		}
	}
}
