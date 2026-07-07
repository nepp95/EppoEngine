#include "pch.h"
#include "Scripting/ScriptInstance.h"

namespace Eppo
{
    ScriptInstance::ScriptInstance(EppoScriptCore::Assembly& assembly, const UUID& entityId, const int32_t classIndex)
        : m_Assembly(&assembly), m_EntityId(entityId), m_ClassIndex(classIndex)
    {}

    auto ScriptInstance::InvokeOnCreate() -> void
    {
        m_Assembly->InvokeOnCreate(static_cast<uint64_t>(m_EntityId));
    }

    auto ScriptInstance::InvokeOnUpdate(const float timestep) -> void
    {
        m_Assembly->InvokeOnUpdate(static_cast<uint64_t>(m_EntityId), timestep);
    }

    auto ScriptInstance::InvokeOnDestroy() -> void
    {
        m_Assembly->InvokeOnDestroy(static_cast<uint64_t>(m_EntityId));
    }

    auto ScriptInstance::GetFieldValue(const int32_t fieldIndex, void* data) const -> void
    {
        m_Assembly->GetFieldValue(static_cast<uint64_t>(m_EntityId), fieldIndex, data);
    }

    auto ScriptInstance::SetFieldValue(const int32_t fieldIndex, const void* data) -> void
    {
        m_Assembly->SetFieldValue(static_cast<uint64_t>(m_EntityId), fieldIndex, data);
    }
}
