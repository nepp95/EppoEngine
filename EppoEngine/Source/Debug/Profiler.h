#pragma once

#if defined(TRACY_ENABLE)
	#include <tracy/Tracy.hpp>
	#define EPPO_PROFILE_FUNCTION(name) ZoneScopedN(name)
	#define EPPO_PROFILE_FRAME_MARK FrameMark
	#define EPPO_PROFILE_GPU(context, cmd, name) TracyVkZone(context, cmd, name)
	#define EPPO_PROFILE_GPU_END(context, cmd) TracyVkCollect(context, cmd)
	#define EPPO_PROFILE_GPU_DESTROY(context) { EPPO_MEM_WARN("Releasing tracy context {}", (void*)context); TracyVkDestroy(context) }
#else
	#define EPPO_PROFILE_FUNCTION(name)
	#define EPPO_PROFILE_FRAME_MARK
	#define EPPO_PROFILE_GPU(context, cmd, name)
	#define EPPO_PROFILE_GPU_END(context, cmd)
	#define EPPO_PROFILE_GPU_DESTROY(context)
#endif
