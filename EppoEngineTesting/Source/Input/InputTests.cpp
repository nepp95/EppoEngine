#include "Support/EppoTest.h"
#include "Support/GlmCheck.h"

#include "Core/Input.h"
#include "Core/SimulatedInput.h"

using namespace Eppo;

// The Input facade delegates to a swappable InputBackend. Installing a
// SimulatedInput lets us exercise the whole query surface with no window or GPU,
// which is exactly the seam automated scenarios use to mimic a user.
SUITE(Input)
{
    // Installs a SimulatedInput as the active backend for the duration of a case
    // and restores the default (GLFW) backend on teardown, so a test never leaks
    // its simulated state into the rest of the runner.
    struct SimulatedInputFixture
    {
        SimulatedInputFixture() { Input::SetBackend(&sim); }
        ~SimulatedInputFixture() { Input::SetBackend(nullptr); }

        SimulatedInput sim;
    };

    TEST_FIXTURE(SimulatedInputFixture, KeyPressReflectsSimulatedState)
    {
        CHECK(!Input::IsKeyPressed(Key::W));

        sim.PressKey(Key::W);
        CHECK(Input::IsKeyPressed(Key::W));
        CHECK(!Input::IsKeyPressed(Key::A));

        sim.ReleaseKey(Key::W);
        CHECK(!Input::IsKeyPressed(Key::W));
    }

    TEST_FIXTURE(SimulatedInputFixture, MultipleKeysHeldSimultaneously)
    {
        sim.PressKey(Key::W);
        sim.PressKey(Key::LeftShift);

        CHECK(Input::IsKeyPressed(Key::W));
        CHECK(Input::IsKeyPressed(Key::LeftShift));
        CHECK(!Input::IsKeyPressed(Key::S));
    }

    TEST_FIXTURE(SimulatedInputFixture, MouseButtonPressReflectsSimulatedState)
    {
        CHECK(!Input::IsMouseButtonPressed(Mouse::ButtonLeft));

        sim.PressMouseButton(Mouse::ButtonLeft);
        CHECK(Input::IsMouseButtonPressed(Mouse::ButtonLeft));
        CHECK(!Input::IsMouseButtonPressed(Mouse::ButtonRight));

        sim.ReleaseMouseButton(Mouse::ButtonLeft);
        CHECK(!Input::IsMouseButtonPressed(Mouse::ButtonLeft));
    }

    TEST_FIXTURE(SimulatedInputFixture, MousePositionRoundTrips)
    {
        sim.SetMousePosition({ 320.0f, 240.0f });

        CHECK_VEC2_CLOSE(glm::vec2(320.0f, 240.0f), Input::GetMousePosition(), 1e-6f);
        CHECK_CLOSE(320.0f, Input::GetMouseX(), 1e-6f);
        CHECK_CLOSE(240.0f, Input::GetMouseY(), 1e-6f);
    }

    TEST_FIXTURE(SimulatedInputFixture, ResetClearsAllState)
    {
        sim.PressKey(Key::Space);
        sim.PressMouseButton(Mouse::ButtonMiddle);
        sim.SetMousePosition({ 10.0f, 20.0f });

        sim.Reset();

        CHECK(!Input::IsKeyPressed(Key::Space));
        CHECK(!Input::IsMouseButtonPressed(Mouse::ButtonMiddle));
        CHECK_VEC2_CLOSE(glm::vec2(0.0f, 0.0f), Input::GetMousePosition(), 1e-6f);
    }

    // Reverting to the default backend must detach the simulated instance; this
    // guards the teardown contract the fixture relies on. Uses the fixture so
    // the backend is restored to default even if an assertion aborts the case
    // (leaving a dangling &sim installed would corrupt every later suite).
    TEST_FIXTURE(SimulatedInputFixture, SetBackendNullRevertsToDefault)
    {
        sim.PressKey(Key::Z);
        CHECK(Input::IsKeyPressed(Key::Z));

        Input::SetBackend(nullptr);
        // The default backend is the GLFW one; we can't query it without a window,
        // but we can assert the facade no longer points at our simulated instance.
        CHECK(&Input::GetBackend() != static_cast<InputBackend*>(&sim));
    }
}
