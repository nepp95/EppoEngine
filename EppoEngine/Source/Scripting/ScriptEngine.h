#pragma once

#include "Scene/Entity.h"
#include "Scripting/Assembly.h"
#include "Scripting/ScriptClass.h"
#include "Scripting/ScriptField.h"
#include "Scripting/ScriptInstance.h"

namespace Eppo
{
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

    // Field values for one entity's script, keyed by field name so they survive
    // reordering/recompilation of the user assembly.
    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;

    // Number of bytes a field type occupies in the marshalling buffer.
    [[nodiscard]] auto ScriptFieldTypeSize(ScriptFieldType type) -> uint32_t;

    // The script engine owns itself between Init() and Shutdown(): Init creates
    // the single instance and Shutdown destroys it. Everything else is an
    // instance method, reached through Get() once the engine is initialized —
    // so the .NET runtime and field storage are plain members, not statics.
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

        // Access the engine. Only valid between Init() and Shutdown(); guard with
        // IsInitialized() at sites that can run before a project is loaded.
        [[nodiscard]] static auto Get() -> ScriptEngine&;

        // Assembly management
        auto LoadUserAssembly(const std::filesystem::path& path) const -> void;
        auto UnloadUserAssembly() -> void;
        [[nodiscard]] auto IsRuntimeLoaded() const -> bool;

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

        // The engine owns the live-instance registry: it is authoritative for
        // which entities have a running script. Returns nullptr when the entity
        // has no live instance (not playing, or its script failed to instantiate).
        // Editing fields through the returned handle mutates the running script
        // directly and deliberately bypasses the serialized side table, so
        // stopping play restores the editor-time values.
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
        std::unordered_map<UUID, ScriptFieldMap> m_FieldStorage;

        // Non-owning; owned by the editor/runtime. Only valid during play.
        WeakRef<Scene> m_SceneContext;

        // The authoritative registry of live script instances, keyed by entity
        // UUID. Populated on play (OnCreateEntity), cleared on stop / assembly
        // unload. The managed runtime only holds the object bodies.
        std::unordered_map<UUID, ScriptInstance> m_EntityInstances;

        static ScopedPtr<ScriptEngine> s_Instance;
    };
}
