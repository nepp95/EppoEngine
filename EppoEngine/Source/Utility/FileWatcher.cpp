#include "pch.h"
#include "Utility/FileWatcher.h"

#include <efsw/efsw.hpp>

#include <atomic>

namespace Eppo
{
    struct FileWatcher::Impl final : efsw::FileWatchListener
    {
        std::atomic<bool> Changed{ false };
        FileWatchFilter Filter;
        efsw::WatchID Id = 0;

        // Declared last so it destructs first: ~FileWatcher must join efsw's thread
        // before the members that thread touches are torn down.
        efsw::FileWatcher Watcher;

        auto handleFileAction(efsw::WatchID, const std::string& dir, const std::string& filename, efsw::Action, const std::string&)
            -> void override
        {
            if (Filter && !Filter(std::filesystem::path(dir) / filename))
                return;

            Changed.store(true, std::memory_order_relaxed);
        }
    };

    FileWatcher::FileWatcher(const std::filesystem::path& directory, FileWatchFilter filter)
        : m_Impl(std::make_unique<Impl>())
    {
        m_Impl->Filter = std::move(filter);

        if (!FS::Exists(directory))
        {
            Log::Error("Cannot watch '{}': directory does not exist", directory);
            return;
        }

        m_Impl->Id = m_Impl->Watcher.addWatch(directory.string(), m_Impl.get(), true);
        if (m_Impl->Id < 0)
        {
            Log::Error("Failed to watch '{}' for changes", directory);
            return;
        }

        m_Impl->Watcher.watch();
    }

    // Out of line: Impl is incomplete in the header.
    FileWatcher::~FileWatcher() = default;

    auto FileWatcher::ConsumeChange() -> bool
    {
        return m_Impl->Changed.exchange(false, std::memory_order_relaxed);
    }
}
