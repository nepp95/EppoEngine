#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

using namespace Eppo;

// Foundation for the scenario tests: boots the real Application and steps frames.
// Needs a display + GPU (label `graphical`, excluded on headless CI).
SUITE(App)
{
    TEST(Application_Boot_ProducesWindowAndDevice)
    {
        Application* app = Testing::AppHarness::Get();
        REQUIRE CHECK(app != nullptr);

        // Bring-up must have produced a live window and device.
        CHECK(app->GetWindow() != nullptr);
        CHECK(app->GetDeviceManager() != nullptr);
        CHECK(app->IsRunning());
    }

    TEST(Application_AdvanceFrames_StaysRunning)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // Drive a batch of real frames; the app must survive and keep running
        // (nothing in this bare harness requests close).
        Testing::AppHarness::AdvanceFrames(30);

        CHECK(Testing::AppHarness::Get()->IsRunning());
    }

    TEST(Application_RepeatedAdvance_StaysRunning)
    {
        if (!Testing::AppHarness::IsAvailable())
            return;

        // A second batch reuses the same persisted app instance (boot-once per
        // process); stepping again must not crash or tear anything down.
        Testing::AppHarness::AdvanceFrames(15);
        CHECK(Testing::AppHarness::Get()->IsRunning());
    }
}
