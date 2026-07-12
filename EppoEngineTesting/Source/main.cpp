#include "pch.h"

#include <UnitTest++/UnitTest++.h>
#include <UnitTest++/TestReporterStdout.h>

#include <cstring>

// Runs a single suite (first arg) so CTest can register one entry per suite and
// label them by cost; with no arg, runs everything.
auto main(int argc, char** argv) -> int
{
    // The editor's entry point (Core/EntryPoint.h) does this; the test runner has
    // its own main(), so initialize logging here or Log:: calls deref null sinks.
    Eppo::Log::Init();

    if (argc < 2)
        return UnitTest::RunAllTests();

    const char* suite = argv[1];
    UnitTest::TestReporterStdout reporter;
    UnitTest::TestRunner runner(reporter);
    return runner.RunTestsIf(UnitTest::Test::GetTestList(), suite,
        [](UnitTest::Test*) { return true; }, 0);
}
