#pragma once

#include "Core/Layer.h"
#include "Event/Event.h"

namespace Eppo
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		virtual ~ImGuiLayer() override = default;

		virtual void InitAPI() = 0;
		virtual void DestroyAPI() = 0;

		void OnAttach() override;
		void OnDetach() override;

		void OnEvent(Event& e) override;

		virtual void Begin() = 0;
		virtual void End() = 0;

		void BlockEvents(bool block) { m_BlockEvents = block; }

		static Ref<ImGuiLayer> Create();

	protected:
		bool m_BlockEvents = true;
	};
}
