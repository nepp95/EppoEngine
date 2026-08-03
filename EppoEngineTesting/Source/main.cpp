#include "pch.h"

#include "Support/AppHarness.h"

#include <gtest/gtest.h>

namespace Eppo
{
    namespace
    {
        // Visual Studio's Test Explorer launches the exe from its own output directory, but the
        // suites resolve Resources/, Projects/ and TestData/ relative to the working directory (as
        // the editor does from EppoEditor/). Point it there so the suites pass however they are
        // launched — Test Explorer, CTest (which also sets it), or a direct run. EP_TEST_WORKING_DIR
        // is baked in by premake to the absolute EppoEditor path.
        auto SetResourceWorkingDirectory() -> void
        {
#ifdef EP_TEST_WORKING_DIR
            std::error_code error;
            std::filesystem::current_path(EP_TEST_WORKING_DIR, error);
            if (error)
                Log::Error("Failed to set the test working directory to '{}': {}", EP_TEST_WORKING_DIR, error.message());
#endif
        }
    }
}

// Google Test discovers and filters tests itself (--gtest_list_tests / --gtest_filter),
// so the runner just wires logging around it and tears the graphical harness down at the end.
auto main(int argc, char** argv) -> int
{
    // The editor's entry point (Core/EntryPoint.h) does this; the test runner has
    // its own main(), so initialize logging here or Log:: calls deref null sinks.
    Eppo::Log::Init();
    Eppo::SetResourceWorkingDirectory();

    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    // Tear the graphical harness down here (if any suite booted it), while the
    // engine loggers are still alive — not during static destruction.
    Eppo::Testing::AppHarness::Shutdown();

    return result;
}
