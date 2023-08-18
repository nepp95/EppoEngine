#pragma once

#include "Core/Base.h"

#ifdef EPPO_ENABLE_ASSERTS
	#define EPPO_ASSERT(condition) { if (!condition) __debugbreak(); }
#else
	#define EPPO_ASSERT(condition)
#endif
