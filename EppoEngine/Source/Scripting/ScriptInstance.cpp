#include "pch.h"
#include "Scripting/ScriptInstance.h"

namespace Eppo
{
    ScriptInstance::ScriptInstance(EppoScriptCore::Assembly& assembly, const UUID& entityId, const int32_t classIndex)
        : m_Assembly(&assembly), m_EntityId(entityId), m_ClassIndex(classIndex)
    {}

    auto ScriptInstance::InvokeOnCreate() -> void
    {
        if (m_Assembly)
            m_Assembly->InvokeOnCreate(static_cast<uint64_t>(m_EntityId));
        else
            Log::Warn("Trying ScriptInstance::InvokeOnCreate but assembly is null!");
    }

    auto ScriptInstance::InvokeOnUpdate(const float timestep) -> void
    {
        if (m_Assembly)
            m_Assembly->InvokeOnUpdate(static_cast<uint64_t>(m_EntityId), timestep);
        else
            Log::Warn("Trying ScriptInstance::InvokeOnUpdate but assembly is null!");
    }

    auto ScriptInstance::InvokeOnDestroy() -> void
    {
        if (m_Assembly)
            m_Assembly->InvokeOnDestroy(static_cast<uint64_t>(m_EntityId));
        else
            Log::Warn("Trying ScriptInstance::InvokeOnDestroy but assembly is null!");
    }

    auto ScriptInstance::GetFieldValue(const int32_t fieldIndex, void* data) const -> void
    {
        if (m_Assembly)
            m_Assembly->GetFieldValue(static_cast<uint64_t>(m_EntityId), fieldIndex, data);
        else
            Log::Warn("Trying ScriptInstance::GetFieldValue but assembly is null!");
    }

    auto ScriptInstance::SetFieldValue(const int32_t fieldIndex, const void* data) -> void
    {
        if (m_Assembly)
            m_Assembly->SetFieldValue(static_cast<uint64_t>(m_EntityId), fieldIndex, data);
        else
            Log::Warn("Trying ScriptInstance::SetFieldValue but assembly is null!");
    }
}
