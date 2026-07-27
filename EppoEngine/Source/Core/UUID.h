#pragma once

#include <cstdint>
#include <unordered_map>

namespace Eppo
{
    class UUID
    {
    public:
        UUID();
        UUID(uint64_t id);
        ~UUID() = default;

        auto operator==(const UUID& other) const -> bool { return m_UUID == other.m_UUID; }
        auto operator!=(const UUID& other) const -> bool { return !(*this == other); }
        auto operator<(const UUID& other) const -> bool { return m_UUID < other.m_UUID; }
        explicit operator bool() const { return m_UUID != 0; }
        explicit operator uint64_t() const { return m_UUID; }

    private:
        uint64_t m_UUID = 0;
    };
}

template<>
struct std::hash<Eppo::UUID>
{
    auto operator()(const Eppo::UUID& uuid) const noexcept -> std::size_t { return hash<uint64_t>()(static_cast<uint64_t>(uuid)); }
};