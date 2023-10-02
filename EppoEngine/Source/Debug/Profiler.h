#pragma once

#if defined(EPPO_ENABLE_PROFILING)
	#include <tracy/Tracy.hpp>
	#define EPPO_PROFILE_FUNCTION(name) ZoneScopedN(name)
	#define EPPO_PROFILE_FRAME_MARK FrameMark
	#define EPPO_PROFILE_GPU(context, commandBuffer, name) TracyVkZone(context, commandBuffer, name)
	#define EPPO_PROFILE_GPU_END(context, commandBuffer) TracyVkCollect(context, commandBuffer)
	#define EPPO_PROFILE_PLOT(name, var) TracyPlot(name, var)
#else
	#define EPPO_PROFILE_FUNCTION(name)
	#define EPPO_PROFILE_FRAME_MARK
	#define EPPO_PROFILE_GPU
	#define EPPO_PROFILE_GPU_END
	#define EPPO_PROFILE_PLOT(name, var)
#endif
