#include "Support/EppoTest.h"

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

    if (argc > 1)
    {
        UnitTest::TestReporterStdout reporter;
        const UnitTest::TestRunner runner(reporter);
        return runner.RunTestsIf(UnitTest::Test::GetTestList(), argv[1], UnitTest::True(), 0);
    }

    return UnitTest::RunAllTests();
}
