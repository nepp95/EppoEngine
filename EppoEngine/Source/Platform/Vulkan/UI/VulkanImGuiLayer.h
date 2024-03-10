#pragma once

#include "ImGui/ImGuiLayer.h"

namespace Eppo
{
	class VulkanImGuiLayer : public ImGuiLayer
	{
	public:
		VulkanImGuiLayer() = default;
		~VulkanImGuiLayer() = default;

		void InitAPI() override;
		void DestroyAPI() override;

		void Begin() override;
		void End() override;
	};
}
