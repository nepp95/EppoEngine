#include "pch.h"
#include "Scripting/ScriptEngine.h"

#include "Core/Input.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Components.h"

namespace Eppo
{
    namespace
    {
        // EppoScriptCore's Assembly API takes .NET hostfxr `char_t` strings, which
        // are wchar_t on Windows and char on Linux. Hand it the matching encoding
        // for the current platform rather than hardcoding wstring().
        auto NativePath(const std::filesystem::path& path)
        {
#ifdef EP_PLATFORM_WINDOWS
            return path.wstring();
#else
            return path.string();
#endif
        }
    }

    std::unique_ptr<ScriptEngine> ScriptEngine::s_Instance = nullptr;

    ScriptEngine::~ScriptEngine()
    {
        m_CoreAssembly.reset();
    }

    auto ScriptEngine::Init(const std::filesystem::path& runtimeConfigPath) -> bool
    {
        EP_PROFILE_FN("ScriptEngine::Init");

        if (s_Instance)
            return true;

        s_Instance = std::unique_ptr<ScriptEngine>(new ScriptEngine());

        EppoScriptCore::NativeCallbacks callbacks;
        callbacks.Log = LogCallback;
        callbacks.InputIsKeyDown = InputIsKeyDownCallback;
        callbacks.ApplyLinearImpulse = ApplyLinearImpulseCallback;
        callbacks.GetLinearVelocity = GetLinearVelocityCallback;
        callbacks.SetLinearVelocity = SetLinearVelocityCallback;

        s_Instance->m_CoreAssembly = std::make_unique<EppoScriptCore::Assembly>(
            ErrorCallback,
            NativePath(runtimeConfigPath),
            callbacks
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

    auto ScriptEngine::LoadUserAssembly(const std::filesystem::path& path) -> void
    {
        EP_PROFILE_FN("ScriptEngine::LoadUserAssembly");

        m_CoreAssembly->LoadUserAssembly(NativePath(path));
    }

    auto ScriptEngine::UnloadUserAssembly() -> void
    {
        EP_PROFILE_FN("ScriptEngine::UnloadUserAssembly");

        // Live instances reference the assembly's managed bodies; drop the
        // registry before those become invalid.
        m_EntityInstances.clear();
        m_CoreAssembly->UnloadUserAssembly();
    }

    auto ScriptEngine::IsRuntimeLoaded() const -> bool
    {
        return m_CoreAssembly != nullptr;
    }

    auto ScriptEngine::GetClasses() const -> const std::vector<EppoScriptCore::ScriptClass>&
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

    auto ScriptEngine::LogCallback(const uint8_t level, const char* message) -> void
    {
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

    auto ScriptEngine::InputIsKeyDownCallback(const uint32_t keyCode) -> bool
    {
        return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
    }

    auto ScriptEngine::ErrorCallback(const std::string& message) -> void
    {
        Log::Error("{}", message);
    }

    auto ScriptEngine::ApplyLinearImpulseCallback(const uint64_t entityId, const EppoScriptCore::EppoVec3 impulse) -> void
    {
        if (!s_Instance || !s_Instance->m_ActivePhysicsWorld)
            return;

        s_Instance->m_ActivePhysicsWorld->ApplyLinearImpulse(UUID(entityId), { impulse.x, impulse.y, impulse.z });
    }

    auto ScriptEngine::GetLinearVelocityCallback(const uint64_t entityId, EppoScriptCore::EppoVec3* outVelocity) -> void
    {
        if (!outVelocity)
            return;

        *outVelocity = { 0.0f, 0.0f, 0.0f };
        if (!s_Instance || !s_Instance->m_ActivePhysicsWorld)
            return;

        const glm::vec3 velocity = s_Instance->m_ActivePhysicsWorld->GetLinearVelocity(UUID(entityId));
        *outVelocity = { velocity.x, velocity.y, velocity.z };
    }

    auto ScriptEngine::SetLinearVelocityCallback(const uint64_t entityId, const EppoScriptCore::EppoVec3 velocity) -> void
    {
        if (!s_Instance || !s_Instance->m_ActivePhysicsWorld)
            return;

        s_Instance->m_ActivePhysicsWorld->SetLinearVelocity(UUID(entityId), { velocity.x, velocity.y, velocity.z });
    }
}
