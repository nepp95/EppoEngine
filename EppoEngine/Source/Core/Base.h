#pragma once

#include <memory>

#if defined(EPPO_DEBUG)
	#define EPPO_ENABLE_ASSERTS
	#define EPPO_ENABLE_VERIFY
#elif defined(EPPO_RELEASE)
	#define EPPO_ENABLE_VERIFY
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