#pragma once

#include "Renderer/Image.h"

#include <imgui.h>

namespace Eppo::UI
{
	void Image(Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f));
	bool ImageButton(const std::string& id, Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f));
	bool ImageButton(Ref<Eppo::Image> image, const ImVec2& imageSize, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f));
}
