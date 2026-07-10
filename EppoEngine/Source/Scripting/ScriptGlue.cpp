#include "pch.h"
#include "Scripting/ScriptGlue.h"

#include "Core/Input.h"

namespace Eppo
{
    struct NativeCallbacks
    {

    };

    NativeCallbacks* ScriptGlue::s_NativeCallbacks = nullptr;

    auto ScriptGlue::GetNativeCallbacks() -> NativeCallbacks*
    {
        if (!s_NativeCallbacks)
            s_NativeCallbacks = new NativeCallbacks();
        return s_NativeCallbacks;
    }

    auto ScriptGlue::Log(const uint8_t level, const char* message) -> void
    {
        EP_ASSERT(level <= 3);

        switch (level)
        {
            case 0:
            {
                Log::Trace("{}", message);
                break;
            }

            case 1:
            {
                Log::Info("{}", message);
                break;
            }

            case 2:
            {
                Log::Warn("{}", message);
                break;
            }

            case 3:
            default:
            {
                Log::Error("{}", message);
                break;
            }
        }
    }

    auto ScriptGlue::Input_IsKeyPressed(const uint16_t keyCode) -> bool
    {
        return Input::IsKeyPressed(keyCode);
    }
}
