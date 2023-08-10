#pragma once

typedef struct VkImage_T* VkImage;
typedef struct VkImageView_T* VkImageView;
typedef struct VkSampler_T* VkSampler;
typedef struct VmaAllocation_T* VmaAllocation;
enum VkImageLayout;

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

	struct ImageInfo
	{
		VkImage Image;
		VkImageView ImageView;
		VkImageLayout ImageLayout;
		VkSampler Sampler;
		VmaAllocation Allocation;
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
	}

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

	private:
		ImageSpecification m_Specification;
		ImageInfo m_Info;
	};
}
