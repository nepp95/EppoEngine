#include "pch.h"
#include "Scripting/ScriptEngine.h"

#include "Project/Project.h"
#include "Utility/Process.h"

namespace Eppo
{
    ScopedPtr<ScriptEngine> ScriptEngine::s_Instance = nullptr;

    namespace
    {
        auto IsUserScriptFile(const std::filesystem::path& path) -> bool
        {
            if (path.extension() != ".cs")
                return false;

            // dotnet writes generated sources into obj/ inside the watched tree, so
            // an unfiltered watch would see its own build output and rebuild forever.
            for (const auto& segment : path)
            {
                if (segment == "obj" || segment == "bin")
                    return false;
            }

            return true;
        }
    }

    // Marshalling width of each field type, matching the managed layout (C# char
    // is UTF-16 → 2 bytes). Sizes field/argument buffers.
    auto ScriptFieldTypeSize(const ScriptFieldType type) -> uint32_t
    {
        switch (type)
        {
            case ScriptFieldType::Float:
                return 4;
            case ScriptFieldType::Double:
                return 8;
            case ScriptFieldType::Bool:
                return 1;
            case ScriptFieldType::Char:
                return 2;
            case ScriptFieldType::Int16:
                return 2;
            case ScriptFieldType::Int32:
                return 4;
            case ScriptFieldType::Int64:
                return 8;
            case ScriptFieldType::Byte:
                return 1;
            case ScriptFieldType::UInt16:
                return 2;
            case ScriptFieldType::UInt32:
                return 4;
            case ScriptFieldType::UInt64:
                return 8;
            case ScriptFieldType::Vector2:
                return 8;
            case ScriptFieldType::Vector3:
                return 12;
            case ScriptFieldType::Vector4:
                return 16;
            case ScriptFieldType::Entity:
                return 8;
            case ScriptFieldType::None:
            default:
                return 0;
        }
    }

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

        // Construct directly (not CreateScopedPtr): the constructor is private, so
        // make_unique cannot reach it, but this static member can.
        s_Instance = ScopedPtr<ScriptEngine>(new ScriptEngine());
        s_Instance->m_CoreAssembly = CreateScopedPtr<Assembly>(EP_NativeString(runtimeConfigPath));

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

    auto ScriptEngine::VerifyRuntime() -> void
    {
        if (!m_ScriptWatcher || !Project::GetActive())
            return;

        // Reloading a frame later lets a burst of saves collapse into one build.
        if (m_ScriptWatcher->ConsumeChange())
        {
            m_ReloadPending = true;
            return;
        }

        // A live scene context means play mode.
        if (!m_ReloadPending || GetSceneContext())
            return;

        ReloadProjectAssembly();
        m_ReloadPending = false;
    }

    auto ScriptEngine::Get() -> ScriptEngine&
    {
        EP_ASSERT(s_Instance != nullptr, "ScriptEngine::Get() called before Init()!");
        return *s_Instance;
    }

    auto ScriptEngine::LoadUserAssembly(const std::filesystem::path& path) -> bool
    {
        EP_PROFILE_FN("ScriptEngine::LoadUserAssembly");

        m_UserAssemblyValid = m_CoreAssembly->LoadUserAssembly(EP_NativeString(path));
        return m_UserAssemblyValid;
    }

    auto ScriptEngine::UnloadUserAssembly() -> void
    {
        EP_PROFILE_FN("ScriptEngine::UnloadUserAssembly");

        m_EntityInstances.clear();
        m_CoreAssembly->UnloadUserAssembly();
    }

    auto ScriptEngine::ReloadProjectAssembly() -> bool
    {
        EP_PROFILE_FN("ScriptEngine::ReloadProjectAssembly");

        m_UserAssemblyValid = false;

        const auto project = Project::GetActive();
        if (!project)
        {
            Log::Error(LogSource::Script, "Cannot build scripts without an active project.");
            return false;
        }

        const auto& name = project->GetSpecification().Name;
        const auto scriptsDirectory = Project::GetScriptsDirectory();
        // A project without a script project is a valid state, not a failure: there
        // is nothing to build, so nothing blocks play.
        const auto projectFile = scriptsDirectory / (name + ".csproj");
        if (!FS::Exists(projectFile))
        {
            Log::Info(LogSource::Script, "Project '{}' has no script project; scripting is unavailable.", name);
            m_UserAssemblyValid = true;
            return true;
        }

        // Watch before building, so a project that opens with broken sources still
        // reloads once the user fixes them.
        if (m_WatchedScriptsDirectory != scriptsDirectory)
        {
            m_ScriptWatcher = CreateScopedPtr<FileWatcher>(scriptsDirectory, IsUserScriptFile);
            m_WatchedScriptsDirectory = scriptsDirectory;
        }

        const auto outputDirectory = Project::GetCacheDirectory() / "Scripts";
        const int32_t exitCode = RunProcess(
            "dotnet",
            { "build", projectFile.string(), "-c", "Debug", "-o", outputDirectory.string(),
              // Point the project at this build's core assembly instead of a baked-in
              // path that goes stale when the output layout changes.
              "-p:CoreManagedDll=" + (FS::GetRootDirectory() / "EppoScriptCore.dll").string(), "--nologo" }
        );

        if (exitCode != 0)
        {
            Log::Error(LogSource::Script, "Script build for '{}' failed with exit code {}; see the build output above.", name, exitCode);
            return false;
        }

        const auto assemblyPath = outputDirectory / (name + ".dll");
        if (!FS::Exists(assemblyPath))
        {
            Log::Error(LogSource::Script, "Script build for '{}' produced no assembly at '{}'.", name, assemblyPath);
            return false;
        }

        UnloadUserAssembly();
        if (!LoadUserAssembly(assemblyPath))
        {
            Log::Error(LogSource::Script, "Failed to load script assembly for '{}'.", name);
            return false;
        }

        Log::Info(LogSource::Script, "Loaded script assembly for '{}'.", name);

        return true;
    }

    auto ScriptEngine::IsUserAssemblyValid() -> bool
    {
        return s_Instance && s_Instance->m_UserAssemblyValid;
    }

    auto ScriptEngine::SetSceneContext(const Ref<Scene>& scene) -> void
    {
        if (!scene)
            m_SceneContext.reset();
        else
            m_SceneContext = scene;
    }

    auto ScriptEngine::GetSceneContext() const -> Ref<Scene>
    {
        return m_SceneContext.lock();
    }

    auto ScriptEngine::IsRuntimeLoaded() const -> bool
    {
        // The assembly object can exist while its managed functions failed to bind
        // (e.g. a bad/stale core DLL); only report loaded when scripting is usable.
        return m_CoreAssembly != nullptr && m_CoreAssembly->IsLoaded();
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

    auto ScriptEngine::InvokeMethod(const Entity entity, const ScriptMethod& method, const void* args, void* ret) const -> void
    {
        // Pass the entity's real 64-bit id; UUID's uint64_t conversion is explicit,
        // so cast here rather than let a bare id slip through and miss the instance.
        m_CoreAssembly->InvokeMethod(static_cast<uint64_t>(entity.GetUUID()), method.Index, args, ret);
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
