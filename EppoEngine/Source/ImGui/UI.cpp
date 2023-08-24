#include "pch.h"
#include "UI.h"

#include <backends/imgui_impl_vulkan_custom.h>

namespace Eppo::UI
{
	void Image(const Ref<Eppo::Image>& image, const ImVec2& size)
	{
		const auto& info = image->GetImageInfo();
		const auto textureID = ImGui_ImplVulkan_AddTexture(info.Sampler, info.ImageView, info.ImageLayout);
		ImGui::Image(textureID, size);
	}
}
