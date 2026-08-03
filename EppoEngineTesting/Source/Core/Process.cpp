#include "TestSupport/EppoTest.h"
#include "Utility/Process.h"

#include <filesystem>

using namespace Eppo;

TEST(Core, RunProcess_SucceedingCommand_ReportsZeroExitCode)
{
#if defined(EP_PLATFORM_WINDOWS)
    EXPECT_EQ(0, RunProcess("cmd.exe", { "/d", "/c", "exit", "0" }));
#else
    EXPECT_EQ(0, RunProcess("sh", { "-c", "true" }));
#endif
}

TEST(Core, RunProcess_FailingCommand_ReportsNonZeroExitCode)
{
#if defined(EP_PLATFORM_WINDOWS)
    EXPECT_TRUE(RunProcess("cmd.exe", { "/d", "/c", "exit", "1" }) != 0);
#else
    EXPECT_TRUE(RunProcess("sh", { "-c", "false" }) != 0);
#endif
}

TEST(Core, RunProcess_UnstartableExecutable_ReportsFailure)
{
    EXPECT_EQ(-1, RunProcess("no_such_executable_eppo_probe", {}));
}

// Arguments are passed as a vector, but Windows flattens them into one command
// line: a space-bearing path must survive that round trip as a single argument
// rather than being split into several directories.
TEST(Core, RunProcess_ArgumentContainingSpaces_ArrivesAsOneArgument)
{
    const auto directory = std::filesystem::temp_directory_path() / "Eppo Process Probe";
    std::filesystem::remove_all(directory);

#if defined(EP_PLATFORM_WINDOWS)
    EXPECT_EQ(0, RunProcess("cmd.exe", { "/d", "/c", "mkdir", directory.string() }));
#else
    EXPECT_EQ(0, RunProcess("mkdir", { directory.string() }));
#endif
    EXPECT_TRUE(std::filesystem::is_directory(directory));

    std::filesystem::remove_all(directory);
}
