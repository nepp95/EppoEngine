#pragma once

#if defined(TRACY_ENABLE) && defined(EPPO_ENABLE_PROFILING)
	#include <tracy/Tracy.hpp>
	#define EPPO_PROFILE_FUNCTION(name) ZoneScopedN(name)
	#define EPPO_PROFILE_FRAME_MARK FrameMark
	#define EPPO_PROFILE_GPU(name) TracyGpuZone(name)
	#define EPPO_PROFILE_GPU_END TracyGpuCollect
#else
	#define EPPO_PROFILE_FUNCTION(name)
	#define EPPO_PROFILE_FRAME_MARK
	#define EPPO_PROFILE_GPU
	#define EPPO_PROFILE_GPU_END
#endif
