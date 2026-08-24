#include "pch.h"

#include <tracy/Tracy.hpp>

inline void* StbiMallocTracy(size_t size)
{
	void* p = malloc(size);
	TracyAlloc(p, size);
	return p;
}

inline void* StbiReallocTracy(void* p, size_t newsz)
{
	TracyFree(p);
	void* newP = realloc(p, newsz);
	TracyAlloc(newP, newsz);
	return newP;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(size) StbiMallocTracy(size)
#define STBI_REALLOC(p, newsz) StbiReallocTracy(p, newsz)
#define STBI_FREE(p) (TracyFree(p), free(p))
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
