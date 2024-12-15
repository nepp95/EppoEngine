#pragma once

namespace Eppo
{
	enum class ImageFormat
	{
		None = 0,

		// Color
		RGBA8,

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
		uint32_t Width = 0;
		uint32_t Height = 0;

		ImageFormat Format = ImageFormat::None;
		ImageUsage Usage;

		std::filesystem::path Filepath;

		bool CubeMap = false;

		ImageSpecification() = default;
		ImageSpecification(const std::filesystem::path& filepath)
			: Filepath(filepath)
		{}
	};

	class Image
	{
	public:
		virtual ~Image() {}

		virtual void SetData(void* data, uint32_t channels = 4) = 0;
		virtual void Release() = 0;

		virtual const ImageSpecification& GetSpecification() const = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		static Ref<Image> Create(const ImageSpecification& specification);
	};

	namespace Utils
	{
		inline bool IsDepthFormat(ImageFormat format)
		{
			return format == ImageFormat::Depth;
		}
	}
}
