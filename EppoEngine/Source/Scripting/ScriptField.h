#pragma once

#include "Core/UUID.h"

namespace Eppo
{
    enum class ScriptFieldType : uint8_t
    {
        None = 0,
        Float,
        Double,
        Bool,
        Char,
        Int16,
        Int32,
        Int64,
        Byte,
        UInt16,
        UInt32,
        UInt64,
        Vector2,
        Vector3,
        Vector4,
        Entity,
    };

    struct ScriptFieldValue
    {
        ScriptFieldType Type = ScriptFieldType::None;
        // Aligned so callers may read typed values directly from the widest field buffer.
        alignas(8) std::array<uint8_t, 16> Buffer{};

        template<typename T>
        [[nodiscard]] auto Get() const -> T
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldValue buffer too small for type");
            T value{};
            std::memcpy(&value, Buffer.data(), sizeof(T));
            return value;
        }

        template<typename T>
        auto Set(const T& value) -> void
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldValue buffer too small for type");
            std::memcpy(Buffer.data(), &value, sizeof(T));
        }
    };

    struct ScriptField
    {
        std::string Name;
        ScriptFieldType Type;
    };

    struct ScriptMethod
    {
        std::string Name;
        int32_t Index = -1;
    };

    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;
    using ScriptFieldStorage = std::unordered_map<UUID, ScriptFieldMap>;
}
