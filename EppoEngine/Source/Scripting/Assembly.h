#pragma once

#include "Scripting/ManagedFunctions.h"
#include "Scripting/RuntimeHost.h"
#include "Scripting/ScriptClass.h"

namespace Eppo
{
    class Assembly
    {
    public:
        explicit Assembly(const EP_NativeString& runtimeConfigPath);
        ~Assembly() = default;

        auto LoadUserAssembly(const EP_NativeString& path) -> bool;
        auto UnloadUserAssembly() -> void;

        [[nodiscard]] auto GetClasses() const -> const std::vector<ScriptClass>& { return m_Classes; }
        [[nodiscard]] auto FindClassIndex(const std::string& fullName) const -> int32_t;

        // True once the managed ScriptGlue entry points resolved. When false the
        // runtime came up but its functions could not be bound, so scripting is
        // unavailable and every managed call below is a guarded no-op.
        [[nodiscard]] auto IsLoaded() const -> bool { return m_ManagedFns != nullptr; }

        [[nodiscard]] auto CreateInstance(int32_t classIndex, uint64_t entityId) const -> bool;
        auto DestroyInstance(uint64_t entityId) const -> void;

        auto InvokeOnCreate(uint64_t entityId) const -> void;
        auto InvokeOnUpdate(uint64_t entityId, float timestep) const -> void;
        auto InvokeOnDestroy(uint64_t entityId) const -> void;
        auto InvokeMethod(uint64_t entityId, int32_t methodIndex, const void* args, void* ret) const -> void;

        auto SetFieldValue(uint64_t entityId, int32_t fieldIndex, const void* data) const -> void;
        auto GetFieldValue(uint64_t entityId, int32_t fieldIndex, void* data) const -> void;

    private:
        auto ResolveManagedFunctions() -> bool;
        auto RegisterInternalCalls() const -> void;
        auto RebuildClassIndex() -> void;
        auto TakeManagedString(char* raw) const -> std::string;

    private:
        ScopedPtr<RuntimeHost> m_RuntimeHost = nullptr;
        ScopedPtr<ManagedFunctions> m_ManagedFns = nullptr;

        // Absolute path to EppoScriptCore.dll (next to the runtime config), so the
        // managed load never depends on the process working directory.
        std::filesystem::path m_CoreAssemblyPath;

        std::vector<ScriptClass> m_Classes;
    };
}
