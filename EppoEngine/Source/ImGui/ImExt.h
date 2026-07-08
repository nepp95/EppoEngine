#pragma once

#include <imgui.h>
#include <nvrhi/nvrhi.h>

namespace ImGuiEx
{
	inline auto CreateTextureRef(const nvrhi::TextureHandle& texture) -> ImTextureRef
	{
		ImTextureRef ref;
		ref._TexID = reinterpret_cast<ImU64>(texture.Get());
		return ref;
	}
}