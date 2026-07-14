#pragma once

#include "Core/Layer.h"

#include <functional>

namespace Eppo::Testing
{
	// A real Layer pushed into the harness app so a scenario's per-frame logic runs
	// from the actual frame loop — it only fires if the app genuinely steps a frame.
	class ScenarioLayer final : public Layer
	{
	public:
		auto SetUpdate(std::function<void(float)> update) -> void { m_Update = std::move(update); }
		auto ClearUpdate() -> void { m_Update = nullptr; }

		auto OnUpdate(float timestep) -> void override
		{
			if (m_Update)
				m_Update(timestep);
		}

	private:
		std::function<void(float)> m_Update;
	};
}
