#include "pch.h"

#include "Support/AppHarness.h"

#include <UnitTest++/UnitTest++.h>
#include <UnitTest++/TestReporterStdout.h>

// Runs a single suite (first arg) so CTest can register one entry per suite and
// label them by cost; with no arg, runs everything.
auto main(int argc, char** argv) -> int
{
    // The editor's entry point (Core/EntryPoint.h) does this; the test runner has
    // its own main(), so initialize logging here or Log:: calls deref null sinks.
    Eppo::Log::Init();

    int result;
    if (argc < 2)
    {
        result = UnitTest::RunAllTests();
    }
    else
    {
        const char* suite = argv[1];
        UnitTest::TestReporterStdout reporter;
        UnitTest::TestRunner runner(reporter);
        result = runner.RunTestsIf(UnitTest::Test::GetTestList(), suite,
            [](UnitTest::Test*) { return true; }, 0);
    }

    // Tear the graphical harness down here (if any suite booted it), while the
    // engine loggers are still alive — not during static destruction.
    Eppo::Testing::AppHarness::Shutdown();

    return result;
}
