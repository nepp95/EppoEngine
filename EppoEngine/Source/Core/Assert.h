#pragma once

#include "Core/Base.h"

#ifdef ASSERTS_ENABLED
	#define EPPO_ASSERT(condition) { if (!condition) __debugbreak(); }
#else
	#define EPPO_ASSERT(condition)
#endif