#pragma once

#include "ImGui/ImGuiLayer.h"

namespace Eppo
{
	class OpenGLImGuiLayer : public ImGuiLayer
	{
	public:
		OpenGLImGuiLayer() = default;
		~OpenGLImGuiLayer() = default;

		void InitAPI() override;
		void DestroyAPI() override;

		void Begin() override;
		void End() override;
	};
}
