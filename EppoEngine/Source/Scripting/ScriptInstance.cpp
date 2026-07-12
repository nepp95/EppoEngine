#include "pch.h"
#include "Scripting/ScriptInstance.h"

namespace Eppo
{
    ScriptInstance::ScriptInstance(Assembly& assembly, const UUID& entityId, const int32_t classIndex)
        : m_Assembly(&assembly), m_EntityId(static_cast<uint64_t>(entityId)), m_ClassIndex(classIndex)
    {}

    auto ScriptInstance::InvokeOnCreate() const -> void
    {
        if (m_Assembly)
            m_Assembly->InvokeOnCreate(m_EntityId);
        else
            Log::Warn("Trying ScriptInstance::InvokeOnCreate but assembly is null!");
    }

    auto ScriptInstance::InvokeOnUpdate(const float timestep) const -> void
    {
        if (m_Assembly)
            m_Assembly->InvokeOnUpdate(m_EntityId, timestep);
        else
            Log::Warn("Trying ScriptInstance::InvokeOnUpdate but assembly is null!");
    }

    auto ScriptInstance::InvokeOnDestroy() const -> void
    {
        if (m_Assembly)
            m_Assembly->InvokeOnDestroy(m_EntityId);
        else
            Log::Warn("Trying ScriptInstance::InvokeOnDestroy but assembly is null!");
    }

    auto ScriptInstance::GetFieldValue(const int32_t fieldIndex, void* data) const -> void
    {
        if (m_Assembly)
            m_Assembly->GetFieldValue(m_EntityId, fieldIndex, data);
        else
            Log::Warn("Trying ScriptInstance::GetFieldValue but assembly is null!");
    }

    auto ScriptInstance::SetFieldValue(const int32_t fieldIndex, const void* data) const -> void
    {
        if (m_Assembly)
            m_Assembly->SetFieldValue(m_EntityId, fieldIndex, data);
        else
            Log::Warn("Trying ScriptInstance::SetFieldValue but assembly is null!");
    }
}
