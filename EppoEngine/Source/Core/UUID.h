#pragma once

#include <yaml-cpp/yaml.h>

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

namespace YAML
{
    inline Emitter& operator<<(Emitter& out, const Eppo::UUID& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << static_cast<uint64_t>(v) << YAML::EndSeq;
        return out;
    }

    template<>
    struct convert<Eppo::UUID>
    {
        static bool decode(const Node& node, Eppo::UUID& uuid)
        {
            if (node.IsSequence())
                return false;

            uuid = Eppo::UUID(node[0].as<uint64_t>());

            return true;
        }
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
