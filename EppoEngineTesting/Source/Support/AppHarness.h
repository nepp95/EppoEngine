#pragma once

#include "Core/Application.h"

#include <cstdint>

namespace Eppo::Testing
{
	// Boots one real, on-screen Application per process for the graphical suites and
	// drives it frame by frame. Needs a display + GPU (suites labelled `graphical`).
	class AppHarness
	{
	public:
		// The shared app, booted on first call; nullptr if it can't boot (no display/GPU).
		[[nodiscard]] static auto Get() -> Application*;
		[[nodiscard]] static auto Get(ApplicationParams params) -> Application*;

		[[nodiscard]] static auto IsAvailable() -> bool;

		// Advance `count` frames at a fixed timestep, stopping early on close request.
		static auto AdvanceFrames(uint32_t count, float timestep = 1.0f / 60.0f) -> void;

		// Deterministic teardown, called at end of main() while loggers are still alive.
		static auto Shutdown() -> void;
	};
}
