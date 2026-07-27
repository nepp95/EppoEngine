#include "pch.h"
#include "Core/Base.h"

#if defined(EP_TRACK_MEMORY)
void* operator new(size_t size)
{
    void* block = malloc(size);
    TracyAllocS(block, size, 32);
    return block;
}

void* operator new[](size_t size)
{
    void* block = malloc(size);
    TracyAllocS(block, size, 32);
    return block;
}

void operator delete(void* block) noexcept
{
    TracyFreeS(block, 32);
    free(block);
}

void operator delete[](void* block) noexcept
{
    TracyFreeS(block, 32);
    free(block);
}
#endif