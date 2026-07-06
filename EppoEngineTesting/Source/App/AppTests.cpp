#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

using namespace Eppo;

// App suite (label `graphical`): boots the real, on-screen application via the
// shared AppHarness and steps real frames through it. This is the foundation for
// the scenario tests — if the window comes up and frames advance without a
// crash, the automated-application path works. Requires a display + GPU, so the
// suite is excluded on headless CI (`ctest -LE graphical`).
SUITE(App)
{
    TEST(HarnessBootsApplication)
    {
        Application* app = Testing::AppHarness::Get();
        REQUIRE CHECK(app != nullptr);

        // Bring-up must have produced a live window and device.
        CHECK(app->GetWindow() != nullptr);
        CHECK(app->GetDeviceManager() != nullptr);
        CHECK(app->IsRunning());
    }

    TEST(AdvancesFramesWithoutCrashing)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // Drive a batch of real frames; the app must survive and keep running
        // (nothing in this bare harness requests close).
        Testing::AppHarness::AdvanceFrames(30);

        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(RepeatedAdvanceIsStable)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // A second batch reuses the same persisted app instance (boot-once per
        // process); stepping again must not crash or tear anything down.
        Testing::AppHarness::AdvanceFrames(15);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }
}
