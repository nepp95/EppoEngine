#pragma once

#include "Scene/Entity.h"
#include "Scripting/Assembly.h"
#include "Scripting/ScriptClass.h"
#include "Scripting/ScriptField.h"
#include "Scripting/ScriptInstance.h"
#include "Utility/FileWatcher.h"

namespace Eppo
{
    class PhysicsWorld;

    // Editor-time value of a single script field. Buffer is sized to the widest
    // field type (Vector4 = 16 bytes) so any field fits without allocation.
    struct ScriptFieldValue
    {
        ScriptFieldType Type = ScriptFieldType::None;
        // Aligned to 8 so callers (ImGui, marshalling) may read typed values
        // directly from the buffer without unaligned access.
        alignas(8) std::array<uint8_t, 16> Buffer{};

        template<typename T>
        [[nodiscard]] auto Get() const -> T
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldValue buffer too small for type");
            T value{};
            std::memcpy(&value, Buffer.data(), sizeof(T));
            return value;
        }

        template<typename T>
        auto Set(const T& value) -> void
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldValue buffer too small for type");
            std::memcpy(Buffer.data(), &value, sizeof(T));
        }
    };

    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;
    [[nodiscard]] auto ScriptFieldTypeSize(ScriptFieldType type) -> uint32_t;

    class ScriptEngine
    {
    public:
        ScriptEngine(const ScriptEngine&) = delete;
        ScriptEngine& operator=(const ScriptEngine&) = delete;
        ~ScriptEngine();

        // Lifecycle
        static auto Init(const std::filesystem::path& runtimeConfigPath) -> bool;
        static auto Shutdown() -> void;
        [[nodiscard]] static auto IsInitialized() -> bool;

        auto VerifyRuntime() -> void;

        // Access the engine. Only valid between Init() and Shutdown(); guard with
        // IsInitialized() at sites that can run before a project is loaded.
        [[nodiscard]] static auto Get() -> ScriptEngine&;

        // Assembly management
        auto LoadUserAssembly(const std::filesystem::path& path) const -> void;
        auto UnloadUserAssembly() -> void;
        auto ReloadProjectAssembly() -> bool;

        [[nodiscard]] auto IsRuntimeLoaded() const -> bool;
        [[nodiscard]] static auto IsUserAssemblyValid() -> bool;

        // Class metadata
        [[nodiscard]] auto GetClasses() const -> const std::vector<ScriptClass>&;
        [[nodiscard]] auto FindClassIndex(const std::string& fullName) const -> int32_t;
        [[nodiscard]] auto IsValidScriptClass(const std::string& fullName) const -> bool;

        // Active runtime scene, set on play so internal calls (ScriptGlue) can
        // resolve an entity UUID back to a live Entity/component. Null when not
        // playing; cleared on stop and on assembly unload.
        auto SetSceneContext(const Ref<Scene>& scene) -> void;
        [[nodiscard]] auto GetSceneContext() const -> Ref<Scene>;

        // Per-entity script instance lifecycle. The scene drives these on play.
        auto OnCreateEntity(Entity entity) -> void;
        auto OnUpdateEntity(Entity entity, float timestep) -> void;
        auto OnDestroyEntity(Entity entity) -> void;
        auto InvokeMethod(Entity entity, const ScriptMethod& method, const void* args = nullptr, void* ret = nullptr) const -> void;

        // The physics world the script physics callbacks act on. Held weakly: it
        // expires when the scene drops the world on stop, so callbacks no-op safely.
        [[nodiscard]] auto GetActivePhysicsWorld() const -> Ref<PhysicsWorld> { return m_ActivePhysicsWorld.lock(); }
        auto SetActivePhysicsWorld(const Ref<PhysicsWorld>& world) -> void { m_ActivePhysicsWorld = world; }

        [[nodiscard]] auto GetEntityInstance(const UUID& entityId) -> ScriptInstance*;

        // Editor-time field storage (side table keyed by entity UUID). This is
        // the authoritative, serialized copy; it is pushed into the live managed
        // instance when the entity's script is created.
        [[nodiscard]] auto GetFieldMap(const UUID& entityId) -> ScriptFieldMap&;
        [[nodiscard]] auto TryGetFieldMap(const UUID& entityId) const -> const ScriptFieldMap*;
        auto CopyFieldMap(const UUID& from, const UUID& to) -> void;
        auto RemoveFieldMap(const UUID& entityId) -> void;

    private:
        ScriptEngine() = default;

        ScopedPtr<Assembly> m_CoreAssembly = nullptr;

        ScopedPtr<FileWatcher> m_ScriptWatcher = nullptr;
        std::filesystem::path m_WatchedScriptsDirectory;
        bool m_UserAssemblyValid = false;
        bool m_ReloadPending = false;

        WeakRef<PhysicsWorld> m_ActivePhysicsWorld;
        WeakRef<Scene> m_SceneContext;

        std::unordered_map<UUID, ScriptInstance> m_EntityInstances;
        std::unordered_map<UUID, ScriptFieldMap> m_FieldStorage;

        static ScopedPtr<ScriptEngine> s_Instance;
    };
}
