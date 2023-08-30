#pragma once

#include <imgui.h>

namespace Eppo
{
	class ScopedBegin
	{
	public:
		ScopedBegin(const char* identifier)
		{
			ImGui::Begin(identifier);
		}

		~ScopedBegin()
		{
			ImGui::End();
		}
	};
}
