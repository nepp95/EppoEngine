#pragma once

#include "Core/Base.h"
#include "Core/SimulatedInput.h"
#include "Scene/Scene.h"

#include <functional>

namespace Eppo::Testing
{
	// Scenario-facing façade over the graphical AppHarness. A scenario constructs
	// a TestContext, which ensures the real on-screen application is booted (once
	// per process), installs a SimulatedInput so the scenario can mimic a user,
	// and owns a fresh Scene for entity work.
	//
	// AdvanceFrames drives real frames through the live application, running the
	// scenario's per-frame logic each step — so simulated input flows through the
	// real Input facade into whatever engine code the scenario exercises (e.g.
	// EditorCamera movement). On destruction the default (device) input backend
	// is restored, so a scenario never leaks its simulated state.
	class TestContext
	{
	public:
		TestContext();
		~TestContext();

		TestContext(const TestContext&) = delete;
		TestContext& operator=(const TestContext&) = delete;

		// Whether the underlying graphical application booted (needs display+GPU).
		// Scenarios should early-return when false.
		[[nodiscard]] auto IsAvailable() const -> bool;

		[[nodiscard]] auto GetInput() -> SimulatedInput& { return m_Input; }
		[[nodiscard]] auto GetScene() -> const Ref<Scene>& { return m_Scene; }

		// Advance the live application by `count` frames at a fixed timestep,
		// invoking `perFrame(timestep)` before each stepped frame — the scenario's
		// "game logic": read input, move things, mutate the scene. No-op when the
		// harness is unavailable.
		auto AdvanceFrames(uint32_t count, const std::function<void(float)>& perFrame = {}, float timestep = 1.0f / 60.0f) -> void;

	private:
		SimulatedInput m_Input;
		Ref<Scene> m_Scene;
	};
}
