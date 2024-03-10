#pragma once

#include "Renderer/Image.h"

#include <imgui.h>

namespace Eppo::UI
{
	void Image(Ref<Eppo::Image> image, const ImVec2& size);
}
