#include "pch.h"
#include "Core/Hash.h"

namespace Eppo
{
    namespace
    {
        auto byteswap64(uint64_t value, void* ptr) -> void
        {
            value = ((value & 0xFF00000000000000u) >> 56u) | ((value & 0x00FF000000000000u) >> 40u) |
                ((value & 0x0000FF0000000000u) >> 24u) | ((value & 0x000000FF00000000u) >> 8u) | ((value & 0x00000000FF000000u) << 8u) |
                ((value & 0x0000000000FF0000u) << 24u) | ((value & 0x000000000000FF00u) << 40u) | ((value & 0x00000000000000FFu) << 56u);

            std::memcpy(ptr, &value, sizeof(uint64_t));
        }
    }

    auto Hash::GenerateFnv(const std::string& contents) -> uint64_t
    {
        constexpr uint64_t fnvOffsetBasis = 0xcbf29ce484222325;
        uint64_t hash = fnvOffsetBasis;
        char* contentsPtr = const_cast<char*>(contents.data());

        for (size_t i = 0; i < contents.size(); i++)
        {
            constexpr uint64_t fnvPrime = 0x100000001b3;
            hash ^= *contentsPtr++;
            hash *= fnvPrime;
        }

        byteswap64(hash, &hash);
        return hash;
    }
}