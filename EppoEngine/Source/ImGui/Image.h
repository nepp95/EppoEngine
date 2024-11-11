#pragma once

#include "Renderer/Image.h"
#include "Renderer/Renderer.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

namespace Eppo
{
	namespace UI
	{
		struct ImageInfo
		{
			VkImageLayout Layout;
			VkImageView ImageView;
			VkSampler Sampler;
			VkDescriptorSet DescriptorSet;
		};

		static std::unordered_map<void*, ImageInfo> s_ImageCache;

		inline void Image(Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f))
		{
			const auto& imageInfo = image->GetImageInfo();

			if (s_ImageCache.find((void*)imageInfo.ImageView) == s_ImageCache.end())
			{
				ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];
				info.Layout = imageInfo.ImageLayout;
				info.ImageView = imageInfo.ImageView;
				info.Sampler = imageInfo.Sampler;
				info.DescriptorSet = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, imageInfo.ImageLayout);
			}

			ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];

			ImGui::Image((ImTextureID)info.DescriptorSet, imageSize, uv0, uv1);
		}

		inline bool ImageButton(const std::string& id, Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f))
		{
			const auto& imageInfo = image->GetImageInfo();

			if (s_ImageCache.find((void*)imageInfo.ImageView) == s_ImageCache.end())
			{
				ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];
				info.Layout = imageInfo.ImageLayout;
				info.ImageView = imageInfo.ImageView;
				info.Sampler = imageInfo.Sampler;
				info.DescriptorSet = ImGui_ImplVulkan_AddTexture(imageInfo.Sampler, imageInfo.ImageView, imageInfo.ImageLayout);
			}

			ImageInfo& info = s_ImageCache[(void*)imageInfo.ImageView];

			return ImGui::ImageButton(id.c_str(), (ImTextureID)info.DescriptorSet, imageSize, uv0, uv1);
		}

		inline bool ImageButton(Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f))
		{
			return ImageButton(image->GetSpecification().Filepath.string(), image, imageSize, uv0, uv1);
		}
	}
}
