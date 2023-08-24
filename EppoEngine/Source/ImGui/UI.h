#pragma once

#include "Renderer/Image.h"

#include <imgui.h>

namespace Eppo::UI
{
	void Image(const Ref<Eppo::Image>& image, const ImVec2& size);
}
