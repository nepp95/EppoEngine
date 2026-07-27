#pragma once

#include <filesystem>
#include <functional>
#include <memory>

namespace Eppo
{
    // Invoked on the watcher's own thread, so it must be thread-safe.
    using FileWatchFilter = std::function<bool(const std::filesystem::path& changedFile)>;

    // Watches a directory tree and raises a flag when anything inside it changes.
    // Polled rather than callback-driven: efsw notifies on its own thread, while the
    // work a caller wants to do in response is main-thread. Polling also collapses
    // the burst of events one editor save produces into a single unit of work.
    class FileWatcher
    {
    public:
        explicit FileWatcher(const std::filesystem::path& directory, FileWatchFilter filter = {});
        ~FileWatcher();

        FileWatcher(const FileWatcher&) = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;

        // True at most once per batch of changes, clearing the flag.
        [[nodiscard]] auto ConsumeChange() -> bool;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
