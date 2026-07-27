#pragma once

#include <span>

namespace Eppo
{
    // Native engine services callable from managed scripts (logging, input, later
    // component access). Each is a plain native function handed to the runtime as
    // a raw pointer during bootstrap; managed calls it by its registered name. To
    // add one: write the function in ScriptGlue.cpp and add a GetInternalCalls() entry.
    class ScriptGlue
    {
    public:
        struct InternalCall
        {
            const char* Name;
            void* Function;
        };
        [[nodiscard]] static auto GetInternalCalls() -> std::span<const InternalCall>;
    };
}
