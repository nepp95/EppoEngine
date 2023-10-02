#include "pch.h"
#include "Base.h"

#if defined(EPPO_TRACK_MEMORY)
	_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
	void* __CRTDECL operator new(size_t size)
	{
		void* block = malloc(size);
		TracyAllocS(block, size, 32);
		return block;
	}

	_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
	void* __CRTDECL operator new[](size_t size)
	{
		void* block = malloc(size);
		TracyAllocS(block, size, 32);
		return block;
	}

	void __CRTDECL operator delete(void* block)
	{
		TracyFreeS(block, 32);
		free(block);
	}

	void __CRTDECL operator delete[](void* block)
	{
		TracyFreeS(block, 32);
		free(block);
	}
#endif
