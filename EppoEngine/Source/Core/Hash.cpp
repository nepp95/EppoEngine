#include "pch.h"
#include "Hash.h"

namespace Eppo
{
	namespace Utils
	{
		static void byteswap64(uint64_t value, void* ptr)
		{
			value =
				((value & 0xFF00000000000000u) >> 56u) |
				((value & 0x00FF000000000000u) >> 40u) |
				((value & 0x0000FF0000000000u) >> 24u) |
				((value & 0x000000FF00000000u) >> 8u)  |
				((value & 0x00000000FF000000u) << 8u)  |
				((value & 0x0000000000FF0000u) << 24u) |
				((value & 0x000000000000FF00u) << 40u) |
				((value & 0x00000000000000FFu) << 56u);

			memcpy(ptr, &value, sizeof(uint64_t));
		}
	}

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
		for (uint32_t i = 0; i < buffer.Size; i++)
		{
			hash ^= *buffer.Data++;
			hash *= fnvPrime;
		}

		// Reverse byte order
		Utils::byteswap64(hash, &hash);

		return hash;
	}
}
