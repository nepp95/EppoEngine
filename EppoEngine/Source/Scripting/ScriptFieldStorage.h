#pragma once

#include "Core/UUID.h"
#include "Scripting/ScriptField.h"

namespace Eppo
{
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

    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;
    using ScriptFieldStorage = std::unordered_map<UUID, ScriptFieldMap>;
}
