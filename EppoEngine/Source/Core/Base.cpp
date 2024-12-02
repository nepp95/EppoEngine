#include "pch.h"
#include "Base.h"

#if defined(EPPO_TRACK_MEMORY)
	[[nodiscard]] void* operator new(size_t size)
	{
		void* block = malloc(size);
		TracyAllocS(block, size, 32);
		return block;
	}

	[[nodiscard]] void* operator new[](size_t size)
	{
		void* block = malloc(size);
		TracyAllocS(block, size, 32);
		return block;
	}

	void operator delete(void* block)
	{
		TracyFreeS(block, 32);
		free(block);
	}

	void operator delete[](void* block)
	{
		TracyFreeS(block, 32);
			free(block);
	}
#endif
