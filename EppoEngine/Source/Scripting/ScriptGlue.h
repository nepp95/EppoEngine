#pragma once

namespace Eppo
{
    struct NativeCallbacks;

    class ScriptGlue
    {
    public:
        static auto GetNativeCallbacks() -> NativeCallbacks*;

        static auto Log(uint8_t level, const char* message) -> void;
        static auto Input_IsKeyPressed(uint16_t keyCode) -> bool;

    private:
        static NativeCallbacks* s_NativeCallbacks;
    };
}
