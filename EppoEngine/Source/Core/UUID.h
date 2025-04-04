#pragma once

namespace Eppo
{
    class UUID
    {
    public:
        UUID();
        UUID(uint64_t uuid);

        explicit operator uint64_t() const
        {
            return m_UUID;
        }

        explicit operator bool() const
        {
            return m_UUID;
        }

        bool operator==(const UUID& other) const
        {
            return m_UUID == other.m_UUID;
        }

        bool operator!=(const UUID& other) const
        {
            return !(*this == other);
        }

        bool operator<(const UUID& other) const
        {
            return m_UUID < other.m_UUID;
        }

    private:
        uint64_t m_UUID;
    };
}

template<>
struct std::hash<Eppo::UUID>
{
    std::size_t operator()(const Eppo::UUID& uuid) const noexcept
    {
        return hash<uint64_t>()(static_cast<uint64_t>(uuid));
    }
};
