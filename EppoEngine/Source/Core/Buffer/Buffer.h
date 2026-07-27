#pragma once

#include <cstdint>

namespace Eppo
{
    struct Buffer
    {
        uint8_t* Data = nullptr;
        uint64_t Size = 0;

        Buffer() = default;

        explicit Buffer(const uint64_t size)
        {
            Size = size;
            Data = new uint8_t[Size];
        }

        explicit Buffer(const Buffer& other, const uint64_t size)
            : Data(other.Data), Size(size)
        {}

        Buffer(uint8_t* data, const uint64_t size)
        {
            Data = data;
            Size = size;
        }

        static auto Copy(const Buffer other) -> Buffer
        {
            const Buffer result(other.Size);
            std::memcpy(result.Data, other.Data, other.Size);
            return result;
        }

        static auto Copy(const uint8_t* data, const uint64_t size) -> Buffer
        {
            const Buffer result(size);
            std::memcpy(result.Data, data, size);
            return result;
        }

        auto Allocate(const uint64_t size) -> void
        {
            Release();

            Data = new uint8_t[size];
            Size = size;
        }

        auto Release() -> void
        {
            delete[] Data;
            Data = nullptr;
            Size = 0;
        }

        template<typename T>
        auto As() -> T*
        {
            return reinterpret_cast<T*>(Data);
        }

        template<typename T>
        auto As() const -> const T*
        {
            return reinterpret_cast<const T*>(Data);
        }
    };
}