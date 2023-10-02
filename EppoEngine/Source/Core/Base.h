#pragma once

#include <memory>

#if defined(EPPO_DEBUG)
	#define EPPO_ENABLE_ASSERTS
	#define EPPO_ENABLE_PROFILING
	#define EPPO_ENABLE_VERIFY
#elif defined(EPPO_RELEASE)
	#define EPPO_ENABLE_VERIFY
	#define EPPO_ENABLE_PROFILING
#endif

#if defined(EPPO_TESTING)
	#undef EPPO_ENABLE_PROFILING
#endif

#include <Debug/Profiler.h>

#define BIT(x) (1 << x)
#define BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#if defined(EPPO_TRACK_MEMORY)
	_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
	void* __CRTDECL operator new(size_t size);

	_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
	void* __CRTDECL operator new[](size_t size);
	
	void __CRTDECL operator delete(void* block);
	
	void __CRTDECL operator delete[](void* block);
#endif

namespace Eppo
{
	// Unique pointer wrapper
	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T, typename... Args>
	constexpr Scope<T> CreateScope(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	// Shared pointer wrapper
	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	constexpr Ref<T> CreateRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}

#include "Core/Assert.h"
#include "Core/Log.h"
