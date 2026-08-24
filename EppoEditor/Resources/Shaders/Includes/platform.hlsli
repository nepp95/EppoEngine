#ifndef PLATFORM_HLSLI
#define PLATFORM_HLSLI
    #if defined(TARGET_VULKAN)
        #define PUSH_CONSTANTS [[vk::push_constant]]
    #else
        #define PUSH_CONSTANTS
    #endif
#endif