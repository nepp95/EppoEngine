#pragma once

#include <cstdint>

namespace Eppo
{
    // Non owning buffer that DOES NOT release memory on destruction - Manual memory management is needed!!!
    struct Buffer
    {
        uint8_t* Data = nullptr;
        uint64_t Size = 0;

        Buffer() = default;

        explicit Buffer(const uint64_t size)
            : Size(size)
        {
            Data = new uint8_t[Size];
        }

        explicit Buffer(const Buffer& other, const uint64_t size)
            : Data(other.Data), Size(size)
        {}

        explicit Buffer(uint8_t* data, const uint64_t size)
            : Data(data), Size(size)
        {}

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

    // Owning buffer that releases memory on destruction - When assigned an existing buffer, takes ownership!
    struct ScopedBuffer
    {
        ScopedBuffer() = default;

        explicit ScopedBuffer(const uint64_t size)
            : m_Buffer(size)
        {}

        explicit ScopedBuffer(Buffer&& other) noexcept
            : m_Buffer(other)
        {
            other.Data = nullptr;
            other.Size = 0;
        }

        explicit ScopedBuffer(uint8_t* data, const uint64_t size)
            : m_Buffer(data, size)
        {}

        ~ScopedBuffer() { m_Buffer.Release(); }

        ScopedBuffer(const ScopedBuffer&) = delete;
        auto operator=(const ScopedBuffer&) -> ScopedBuffer& = delete;

        ScopedBuffer(ScopedBuffer&& other) noexcept
            : m_Buffer(other.m_Buffer)
        {
            other.m_Buffer.Data = nullptr;
            other.m_Buffer.Size = 0;
        }

        auto operator=(ScopedBuffer&& other) noexcept -> ScopedBuffer&
        {
            if (this == &other)
                return *this;

            m_Buffer.Release();
            m_Buffer = other.m_Buffer;
            other.m_Buffer.Data = nullptr;
            other.m_Buffer.Size = 0;
            return *this;
        }

        [[nodiscard]] auto Data() const -> uint8_t* { return m_Buffer.Data; }
        [[nodiscard]] auto Size() const -> uint64_t { return m_Buffer.Size; }

        template<typename T>
        auto As() -> T*
        {
            return m_Buffer.As<T>();
        }

    private:
        Buffer m_Buffer;
    };
}
