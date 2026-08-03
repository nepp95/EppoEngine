#include "Support/EppoTest.h"
#include "Utility/Process.h"

#include <filesystem>

using namespace Eppo;

SUITE(Core)
{
    TEST(RunProcess_SucceedingCommand_ReportsZeroExitCode)
    {
#if defined(EP_PLATFORM_WINDOWS)
        CHECK_EQUAL(0, RunProcess("cmd.exe", { "/d", "/c", "exit", "0" }));
#else
        CHECK_EQUAL(0, RunProcess("sh", { "-c", "true" }));
#endif
    }

    TEST(RunProcess_FailingCommand_ReportsNonZeroExitCode)
    {
#if defined(EP_PLATFORM_WINDOWS)
        CHECK(RunProcess("cmd.exe", { "/d", "/c", "exit", "1" }) != 0);
#else
        CHECK(RunProcess("sh", { "-c", "false" }) != 0);
#endif
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

#if defined(EP_PLATFORM_WINDOWS)
        CHECK_EQUAL(0, RunProcess("cmd.exe", { "/d", "/c", "mkdir", directory.string() }));
#else
        CHECK_EQUAL(0, RunProcess("mkdir", { directory.string() }));
#endif
        CHECK(std::filesystem::is_directory(directory));

        std::filesystem::remove_all(directory);
    }
}
