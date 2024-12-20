#pragma once

namespace Eppo
{
	enum class ImageFormat
	{
		None = 0,

		// Color
		RGB16,
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
		ImageSpecification(std::filesystem::path filepath)
			: Filepath(std::move(filepath))
		{}
	};

	class Image
	{
	public:
		virtual ~Image() = default;

		virtual void SetData(void* data, uint32_t channels = 4) = 0;
		virtual void Release() = 0;

		[[nodiscard]] virtual const ImageSpecification& GetSpecification() const = 0;
		[[nodiscard]] virtual uint32_t GetWidth() const = 0;
		[[nodiscard]] virtual uint32_t GetHeight() const = 0;

		static Ref<Image> Create(const ImageSpecification& specification);
	};

	namespace Utils
	{
		inline bool IsDepthFormat(const ImageFormat format)
		{
			return format == ImageFormat::Depth;
		}
	}
}
