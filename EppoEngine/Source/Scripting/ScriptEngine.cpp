#include "pch.h"
#include "Scripting/ScriptEngine.h"

#include "Scene/Components.h"

namespace Eppo
{
    ScopedPtr<ScriptEngine> ScriptEngine::s_Instance = nullptr;

    ScriptEngine::~ScriptEngine()
    {
        m_CoreAssembly.reset();
    }

    auto ScriptEngine::Init(const std::filesystem::path& runtimeConfigPath) -> bool
    {
        EP_PROFILE_FN("ScriptEngine::Init");

        if (s_Instance)
        {
            Log::Warn("Trying to run ScriptEngine::Init whilst it is already initialized!");
            return true;
        }

        s_Instance = CreateScopedPtr<ScriptEngine>();
        s_Instance->m_CoreAssembly = CreateScopedPtr<Assembly>(
            EP_NativeString(runtimeConfigPath)
        );

        return s_Instance->IsRuntimeLoaded();
    }

    auto ScriptEngine::Shutdown() -> void
    {
        EP_PROFILE_FN("ScriptEngine::Shutdown");

        if (!s_Instance)
            return;

        s_Instance->m_CoreAssembly.reset();
        s_Instance.reset();
    }

    auto ScriptEngine::IsInitialized() -> bool
    {
        return s_Instance != nullptr;
    }

    auto ScriptEngine::Get() -> ScriptEngine&
    {
        EP_ASSERT(s_Instance != nullptr, "ScriptEngine::Get() called before Init()!");
        return *s_Instance;
    }

    auto ScriptEngine::LoadUserAssembly(const std::filesystem::path& path) const -> void
    {
        EP_PROFILE_FN("ScriptEngine::LoadUserAssembly");

        m_CoreAssembly->LoadUserAssembly(EP_NativeString(path));
    }

    auto ScriptEngine::UnloadUserAssembly() -> void
    {
        EP_PROFILE_FN("ScriptEngine::UnloadUserAssembly");

        m_EntityInstances.clear();
        m_CoreAssembly->UnloadUserAssembly();
    }

    auto ScriptEngine::IsRuntimeLoaded() const -> bool
    {
        return m_CoreAssembly != nullptr;
    }

    auto ScriptEngine::GetClasses() const -> const std::vector<ScriptClass>&
    {
        return m_CoreAssembly->GetClasses();
    }

    auto ScriptEngine::FindClassIndex(const std::string& fullName) const -> int32_t
    {
        return m_CoreAssembly->FindClassIndex(fullName);
    }

    auto ScriptEngine::IsValidScriptClass(const std::string& fullName) const -> bool
    {
        return FindClassIndex(fullName) >= 0;
    }

    auto ScriptEngine::OnCreateEntity(Entity entity) -> void
    {
        EP_PROFILE_FN("ScriptEngine::OnCreateEntity");

        if (!entity.HasComponent<ScriptComponent>())
            return;

        const auto& sc = entity.GetComponent<ScriptComponent>();
        const auto classIndex = FindClassIndex(sc.ClassName);
        if (classIndex < 0)
        {
            Log::Warn("Entity '{}' references unknown script class '{}'", entity.GetName(), sc.ClassName);
            return;
        }

        const auto& uuid = entity.GetUUID();
        const auto entityId = static_cast<uint64_t>(uuid);

        if (!m_CoreAssembly->CreateInstance(classIndex, entityId))
        {
            Log::Error("Failed to instantiate script '{}' for entity '{}'", sc.ClassName, entity.GetName());
            return;
        }

        // Register the engine-owned handle to the fresh managed instance.
        const auto [instanceIt, _] = m_EntityInstances.try_emplace(uuid, *m_CoreAssembly, uuid, classIndex);
        ScriptInstance& instance = instanceIt->second;

        // Push the stored field values into the fresh managed instance.
        if (const auto storageIt = m_FieldStorage.find(uuid); storageIt != m_FieldStorage.end())
        {
            const auto& fields = GetClasses()[classIndex].GetFields();
            for (int32_t i = 0; i < static_cast<int32_t>(fields.size()); i++)
            {
                if (const auto valueIt = storageIt->second.find(fields[i].Name); valueIt != storageIt->second.end())
                    instance.SetFieldValue(i, valueIt->second.Buffer.data());
            }
        }

        instance.InvokeOnCreate();
    }

    auto ScriptEngine::OnUpdateEntity(Entity entity, const float timestep) -> void
    {
        EP_PROFILE_FN("ScriptEngine::OnUpdateEntity");

        if (!entity.HasComponent<ScriptComponent>())
            return;

        // No live instance means the script never instantiated (a downed runtime
        // is already reported by OnCreateEntity), so stay quiet here to avoid
        // flooding the log every frame.
        const auto it = m_EntityInstances.find(entity.GetUUID());
        if (it == m_EntityInstances.end())
            return;

        it->second.InvokeOnUpdate(timestep);
    }

    auto ScriptEngine::OnDestroyEntity(Entity entity) -> void
    {
        EP_PROFILE_FN("ScriptEngine::OnDestroyEntity");

        if (!entity.HasComponent<ScriptComponent>())
            return;

        const auto& uuid = entity.GetUUID();
        const auto it = m_EntityInstances.find(uuid);
        if (it == m_EntityInstances.end())
            return;

        it->second.InvokeOnDestroy();
        m_CoreAssembly->DestroyInstance(static_cast<uint64_t>(uuid));
        m_EntityInstances.erase(it);
    }

    auto ScriptEngine::GetEntityInstance(const UUID& entityId) -> ScriptInstance*
    {
        const auto it = m_EntityInstances.find(entityId);
        return it != m_EntityInstances.end() ? &it->second : nullptr;
    }

    auto ScriptEngine::GetFieldMap(const UUID& entityId) -> ScriptFieldMap&
    {
        return m_FieldStorage[entityId];
    }

    auto ScriptEngine::TryGetFieldMap(const UUID& entityId) const -> const ScriptFieldMap*
    {
        // "Try" accessor: nullptr means "no stored fields" for this entity.
        // Callers handle nullptr, so this is an expected result, not an error.
        if (m_FieldStorage.contains(entityId))
            return &m_FieldStorage.at(entityId);
        return nullptr;
    }

    auto ScriptEngine::CopyFieldMap(const UUID& from, const UUID& to) -> void
    {
        if (m_FieldStorage.contains(from))
            m_FieldStorage[to] = m_FieldStorage.at(from);
    }

    auto ScriptEngine::RemoveFieldMap(const UUID& entityId) -> void
    {
        m_FieldStorage.erase(entityId);
    }
}
