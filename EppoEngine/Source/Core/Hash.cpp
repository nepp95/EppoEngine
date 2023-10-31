#include "pch.h"
#include "Hash.h"

namespace Eppo
{
	uint64_t Hash::GenerateFnv(const std::string& contents)
	{
		Buffer buffer(contents.size());
		memcpy(buffer.Data, contents.c_str(), contents.size());

		uint64_t hash = GenerateFnv(buffer);
		buffer.Release();

		return hash;
	}

	uint64_t Hash::GenerateFnv(Buffer buffer)
	{
		constexpr uint64_t fnvOffsetBasis = 0xcbf29ce484222325;
		constexpr uint64_t fnvPrime = 0x100000001b3;

		// Generate hash
		uint64_t hash = fnvOffsetBasis;
		uint64_t* hashPtr = &hash;
		for (uint32_t i = 0; i < buffer.Size; i++)
		{
			hash ^= *buffer.Data++;
			hash *= fnvPrime;
		}

		// Reverse byte order
		hash = _byteswap_uint64(hash);

		return hash;
	}
}
