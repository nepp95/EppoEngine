#pragma once

#include "Core/Log.h"

#include <tracy/Tracy.hpp>

#include <csignal>
#include <memory>

#define EP_TRACK_MEMORY
#if defined(EP_TRACK_MEMORY)
	[[nodiscard]] void* operator new(size_t size);
	[[nodiscard]] void* operator new[](size_t size);
	void operator delete(void* block) noexcept;
	void operator delete[](void* block) noexcept;
#endif

namespace Eppo
{
	constexpr auto EP_ASSERT(const bool condition, const char* message = nullptr) -> void
	{
		#if defined(EP_DEBUG) || defined(EP_RELEASE)
			if (!condition)
			{
				if (message)
					Log::Error("Assertion failed: {}", message);
				#if defined(EP_PLATFORM_WINDOWS)
				__debugbreak();
				#elif defined(EP_PLATFORM_LINUX)
				raise(SIGTRAP);
				#endif
			}
		#endif
	}

	#if defined(TRACY_ENABLE)
		#define EP_FRAME_MARK FrameMark
		#define EP_PROFILE_FN(name) ZoneScopedN(name)
	#else
		#define EP_FRAME_MARK
		#define EP_PROFILE_FN(name)
	#endif

	template<typename T>
	using ScopedPtr = std::unique_ptr<T>;

	template<typename T, typename... Args>
	constexpr auto CreateScopedPtr(Args&&... args) -> ScopedPtr<T>
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	constexpr auto CreateRef(Args&&... args) -> Ref<T>
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	// Non-owning observer of a Ref; lock() before use.
	template<typename T>
	using WeakRef = std::weak_ptr<T>;
}
