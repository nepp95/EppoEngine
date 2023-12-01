#include "pch.h"
#include "Base.h"

#if defined(EPPO_TRACK_MEMORY)
	[[nodiscard]] void* __cdecl operator new(size_t size)
	{
		void* block = malloc(size);
		TracyAllocS(block, size, 32);
		return block;
	}

	[[nodiscard]] void* __cdecl operator new[](size_t size)
	{
		void* block = malloc(size);
		TracyAllocS(block, size, 32);
		return block;
	}

	void __cdecl operator delete(void* block)
	{
		TracyFreeS(block, 32);
		free(block);
	}

	void __cdecl operator delete[](void* block)
	{
		TracyFreeS(block, 32);
		free(block);
	}
#endif
