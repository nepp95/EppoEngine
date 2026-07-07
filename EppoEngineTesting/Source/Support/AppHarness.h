#pragma once

#include "Core/Application.h"

#include <cstdint>

namespace Eppo::Testing
{
	// Boots one real, on-screen Application per process for the graphical suites
	// and drives it frame by frame. Construction brings up the GLFW window + the
	// Vulkan device and loads shaders/fonts from the Resources/ directory copied
	// next to the exe, so these suites require a display + GPU; they are labelled
	// `graphical` and excluded on headless CI.
	//
	// The instance is created on first use and lives until process exit (the
	// Application singleton guard permits only one at a time, and bring-up is
	// expensive, so we share it across every scenario in the suite).
	class AppHarness
	{
	public:
		// The shared harness application, booted on first call. Returns nullptr if
		// the app could not be created (e.g. no display/GPU) — callers guard on it.
		[[nodiscard]] static auto Get() -> Application*;

		// True once a boot has been attempted and succeeded.
		[[nodiscard]] static auto IsAvailable() -> bool;

		// Advance the real render loop by `count` frames using a fixed timestep.
		// Stops early if the application requests close (e.g. window closed). No-op
		// when the harness is unavailable.
		static auto AdvanceFrames(uint32_t count, float timestep = 1.0f / 60.0f) -> void;

		// Tear the application down deterministically. Called by the test runner
		// at the end of main() so the GPU/window shut down while the engine
		// loggers are still alive (rather than during static destruction). No-op
		// if never booted.
		static auto Shutdown() -> void;
	};
}
