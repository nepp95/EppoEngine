#include "Support/EppoTest.h"
#include "Utility/Process.h"

#include <filesystem>

using namespace Eppo;

// Driven through `cmake -E`, which is portable across the Windows and Linux
// runners and guaranteed present (CMake builds this suite).
SUITE(Core)
{
    TEST(RunProcess_SucceedingCommand_ReportsZeroExitCode)
    {
        CHECK_EQUAL(0, RunProcess("cmake", { "-E", "true" }));
    }

    TEST(RunProcess_FailingCommand_ReportsNonZeroExitCode)
    {
        CHECK(RunProcess("cmake", { "-E", "false" }) != 0);
    }

    TEST(RunProcess_UnstartableExecutable_ReportsFailure)
    {
        CHECK_EQUAL(-1, RunProcess("no_such_executable_eppo_probe", {}));
    }

    // Arguments are passed as a vector, but Windows flattens them into one command
    // line: a space-bearing path must survive that round trip as a single argument
    // rather than being split into several directories.
    TEST(RunProcess_ArgumentContainingSpaces_ArrivesAsOneArgument)
    {
        const auto directory = std::filesystem::temp_directory_path() / "Eppo Process Probe";
        std::filesystem::remove_all(directory);

        CHECK_EQUAL(0, RunProcess("cmake", { "-E", "make_directory", directory.string() }));
        CHECK(std::filesystem::is_directory(directory));

        std::filesystem::remove_all(directory);
    }
}
