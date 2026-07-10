#pragma once

namespace Eppo
{
    enum class ScriptFieldType : uint8_t
    {
        None = 0,
        Float, Double,
        Bool,
        Char, Int16, Int32, Int64,
        Byte, UInt16, UInt32, UInt64,
        Vector2, Vector3, Vector4,
        Entity,
    };

    struct ScriptField
    {
        std::string Name;
        ScriptFieldType Type;
    };

    struct ScriptMethod
    {
        std::string Name;
    };
}