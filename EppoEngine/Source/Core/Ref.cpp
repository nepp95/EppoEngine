#include "pch.h"
#include "Core/Ref.h"

#include <mutex>
#include <unordered_set>

namespace Eppo
{
    namespace
    {
#ifdef EP_DEBUG
        std::unordered_set<void*> s_LiveReferences;
        std::mutex s_Mutex;
#endif
    }

    auto AddLiveRef(void* instance) -> void
    {
#ifdef EP_DEBUG
        EP_ASSERT(instance);
        std::scoped_lock lock(s_Mutex);
        s_LiveReferences.insert(instance);
#endif
    }

    auto RemoveLiveRef(void* instance) -> void
    {
#ifdef EP_DEBUG
        EP_ASSERT(instance);
        std::scoped_lock lock(s_Mutex);
        EP_ASSERT(s_LiveReferences.contains(instance));
        s_LiveReferences.erase(instance);
#endif
    }

    auto IsLive(void* instance) -> bool
    {
        EP_ASSERT(instance);
#ifdef EP_DEBUG
        std::scoped_lock lock(s_Mutex);
        return s_LiveReferences.contains(instance);
#else
        return true;
#endif
    }
}
