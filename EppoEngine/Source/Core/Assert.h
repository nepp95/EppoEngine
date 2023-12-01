#pragma once

#include "Core/Base.h"

#ifdef EPPO_ENABLE_ASSERTS
	#ifdef EPPO_PLATFORM_WINDOWS
		#define EPPO_ASSERT(condition) { if (!(condition)) __debugbreak(); }
	#elif defined(EPPO_PLATFORM_LINUX)
		#define EPPO_ASSERT(condition) { if (!(condition)) raise(SIGTRAP); }
	#endif
#else
	#define EPPO_ASSERT(condition)
#endif
