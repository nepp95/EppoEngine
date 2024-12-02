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

		void BlockEvents(bool block) { m_BlockEvents = block; }

	private:
		void SetupStyle() const;

	private:
		bool m_BlockEvents = true;
	};
}
