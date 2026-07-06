#include "Support/EppoTest.h"
#include "Support/AppHarness.h"

#include <UnitTest++/UnitTest++.h>
#include <UnitTest++/TestReporterStdout.h>

// Test runner entry point.
//
// With no arguments it runs every registered test (RunAllTests). With a suite
// name argument it runs only that UnitTest++ SUITE, e.g.
//
//     EppoEngineTesting Core
//
// CTest registers one entry per suite (see CMakeLists.txt) so suites get
// independent pass/fail reporting and can be filtered by label, e.g.
// `ctest -L unit`.
int main(int argc, char** argv)
{
    // The engine's loggers are null shared_ptrs until Init() runs; any engine
    // code that logs on an error path (Filesystem, Scene, Scripting, ...) would
    // otherwise dereference null. The runner owns this for every suite.
    Eppo::Log::Init();

    int result;
    if (argc > 1)
    {
        UnitTest::TestReporterStdout reporter;
        const UnitTest::TestRunner runner(reporter);
        result = runner.RunTestsIf(UnitTest::Test::GetTestList(), argv[1], UnitTest::True(), 0);
    }
    else
    {
        result = UnitTest::RunAllTests();
    }

    // Tear down the graphical harness (if any suite booted it) here, while the
    // engine loggers are still alive — not during static destruction.
    Eppo::Testing::AppHarness::Shutdown();

    return result;
}
