#pragma once

#include "Core/Layer.h"

#include <functional>

namespace Eppo::Testing
{
	// A real engine Layer pushed into the harness application so a scenario's
	// per-frame logic is driven by the actual frame loop (Application::StepFrame
	// calls every layer's OnUpdate). This is what makes the scenario tests genuine
	// integration tests: the logic only runs if the live app actually steps a
	// frame, so a broken StepFrame (or a frame the device refuses to begin) means
	// the logic never runs and the scenario's assertions fail.
	//
	// One instance is pushed for the process lifetime; TestContext sets the
	// callback around an AdvanceFrames call and clears it afterwards, so a
	// finished scenario's logic never runs on later frames.
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
