#pragma once

#if defined(TRACY_ENABLE)
    #define EPPO_PROFILE_FUNCTION(name) ZoneScopedN(name)
    #define EPPO_PROFILE_FRAME_MARK FrameMark
    #define EPPO_PROFILE_GPU_SCOPED(context, cmd, name) TracyVkZone(context, cmd, name)
    #define EPPO_PROFILE_GPU_MANUAL(context)
    #define EPPO_PROFILE_GPU_COLLECT(context, cmd) TracyVkCollect(context, cmd)
#else
    #define EPPO_PROFILE_FUNCTION(name)
    #define EPPO_PROFILE_FRAME_MARK
    #define EPPO_PROFILE_GPU(context, cmd, name)
    #define EPPO_PROFILE_GPU_MANUAL(context)
    #define EPPO_PROFILE_GPU_COLLECT(context, cmd)
#endif
