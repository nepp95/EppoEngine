#pragma once

#include "Event/Event.h"

namespace Eppo
{
	class Layer
	{
	public:
		Layer(std::string name = "Layer");
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}

		virtual void Update(float timestep) {}
		virtual void Render() {}
		virtual void RenderGui() {}

		virtual void OnEvent(Event& e) {}

	private:
		std::string m_DebugName;
	};
}
