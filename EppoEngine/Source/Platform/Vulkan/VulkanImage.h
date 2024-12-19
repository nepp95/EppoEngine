#pragma once

#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/Image.h"

namespace Eppo
{
	struct ImageInfo
	{
		VkImage Image = nullptr;
		VkImageView ImageView = nullptr;
		VkImageLayout ImageLayout;
		VkSampler Sampler = nullptr;
		VmaAllocation Allocation = nullptr;
	};

	class VulkanImage : public Image
	{
	public:
		explicit VulkanImage(const ImageSpecification& specification);
		~VulkanImage() final;

		void SetData(void* data, uint32_t channels = 4) override;
		void Release() override;

		[[nodiscard]] const ImageSpecification& GetSpecification() const override { return m_Specification; }

		[[nodiscard]] uint32_t GetWidth() const override { return m_Specification.Width; }
		[[nodiscard]] uint32_t GetHeight() const override { return m_Specification.Height; }

		ImageInfo& GetImageInfo() { return m_ImageInfo; }

		static void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout srcLayout, VkImageLayout dstLayout);
		static VkImageAspectFlags GetImageAspectFlags(VkImageLayout layout);

	private:
		ImageSpecification m_Specification;
		ImageInfo m_ImageInfo;

		bool m_IsHDR = false;

		void* m_ImageData = nullptr;
	};

	namespace Utils
	{
		inline VkFormat ImageFormatToVkFormat(ImageFormat format)
		{
			EPPO_ASSERT(format != ImageFormat::None);

			Ref<VulkanContext> context = VulkanContext::Get();
			Ref<VulkanPhysicalDevice> physicalDevice = context->GetPhysicalDevice();

			return physicalDevice->GetSupportedImageFormat(format);
		}
	}
}
