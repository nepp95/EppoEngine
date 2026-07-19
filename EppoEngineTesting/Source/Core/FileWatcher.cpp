#include "Support/EppoTest.h"
#include "Utility/FileWatcher.h"

#include <chrono>
#include <filesystem>
#include <thread>

using namespace Eppo;

SUITE(Core)
{
    namespace
    {
        // efsw reports from its own thread, so polling keeps the passing case fast
        // and the failing case bounded.
        auto WaitForChange(FileWatcher& watcher, std::chrono::milliseconds timeout) -> bool
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (watcher.ConsumeChange())
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return false;
        }

        struct ScopedDir
        {
            std::filesystem::path Path;

            explicit ScopedDir(const std::string& name)
            {
                Path = std::filesystem::temp_directory_path() / ("EppoWatch_" + name);
                std::filesystem::remove_all(Path);
                FS::CreateDir(Path);
            }

            ~ScopedDir() { std::filesystem::remove_all(Path); }

            ScopedDir(const ScopedDir&) = delete;
            ScopedDir& operator=(const ScopedDir&) = delete;
        };
    }

    TEST(FileWatcher_FileCreated_ReportsChange)
    {
        const ScopedDir dir("Created");
        FileWatcher watcher(dir.Path);

        FS::WriteText(dir.Path / "Script.cs", "public class A { }", true);

        CHECK(WaitForChange(watcher, std::chrono::seconds(5)));
    }

    TEST(FileWatcher_FileModified_ReportsChange)
    {
        const ScopedDir dir("Modified");
        const auto file = dir.Path / "Script.cs";
        FS::WriteText(file, "public class A { }", true);

        FileWatcher watcher(dir.Path);
        CHECK(!watcher.ConsumeChange());

        FS::WriteText(file, "public class A { int x; }", true);

        CHECK(WaitForChange(watcher, std::chrono::seconds(5)));
    }

    // A consumed change must not re-report, or hot reload would loop.
    TEST(FileWatcher_ConsumeChange_ClearsTheFlag)
    {
        const ScopedDir dir("Cleared");
        FileWatcher watcher(dir.Path);

        FS::WriteText(dir.Path / "Script.cs", "public class A { }", true);
        REQUIRE CHECK(WaitForChange(watcher, std::chrono::seconds(5)));

        CHECK(!watcher.ConsumeChange());
    }

    // A project without a Scripts directory is a normal case.
    TEST(FileWatcher_MissingDirectory_StaysInert)
    {
        FileWatcher watcher(std::filesystem::temp_directory_path() / "EppoWatch_DoesNotExist");

        CHECK(!watcher.ConsumeChange());
    }
}
