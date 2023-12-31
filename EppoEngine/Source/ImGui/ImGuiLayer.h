#pragma once

#include "Core/Layer.h"
#include "Event/Event.h"

namespace Eppo
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() override = default;

		void OnAttach() override;
		void OnDetach() override;

		void OnEvent(Event& e) override;

		void Begin();
		void End();
	};
}
