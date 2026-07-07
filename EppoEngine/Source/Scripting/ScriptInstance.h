#pragma once

#include "Core/UUID.h"

#include <EppoScriptCore.Native/Assembly.h>

namespace Eppo
{
    // Engine-owned handle to one live managed script instance. The C# object body
    // lives in the .NET runtime (keyed by entity id); this handle is the engine's
    // authoritative reference to it — the same ownership split Unity uses, where a
    // native object owns its managed wrapper. Created on play and destroyed on
    // stop by ScriptEngine's per-entity registry, which is the single source of
    // truth for "does this entity have a running script".
    class ScriptInstance
    {
    public:
        ScriptInstance(EppoScriptCore::Assembly& assembly, const UUID& entityId, int32_t classIndex);

        auto InvokeOnCreate() -> void;
        auto InvokeOnUpdate(float timestep) -> void;
        auto InvokeOnDestroy() -> void;

        // Field access on the live instance. fieldIndex indexes the owning class's
        // GetFields(); data must point to a buffer matching the field's type width.
        auto GetFieldValue(int32_t fieldIndex, void* data) const -> void;
        auto SetFieldValue(int32_t fieldIndex, const void* data) -> void;

        [[nodiscard]] auto GetClassIndex() const -> int32_t { return m_ClassIndex; }

    private:
        // Non-owning: the Assembly outlives every instance (ScriptEngine owns both
        // and clears the registry before the assembly is torn down).
        EppoScriptCore::Assembly* m_Assembly;
        UUID m_EntityId;
        int32_t m_ClassIndex;
    };
}
