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

        auto LoadUserAssembly(const EP_NativeString& path) -> void;
        auto UnloadUserAssembly() -> void;

        [[nodiscard]] auto GetClasses() const -> const std::vector<ScriptClass>& { return m_Classes; }
        [[nodiscard]] auto FindClassIndex(const std::string& fullName) const -> int32_t;

        [[nodiscard]] auto CreateInstance(int32_t classIndex, uint64_t entityId) const -> bool;
        auto DestroyInstance(uint64_t entityId) const -> void;

        auto InvokeOnCreate(uint64_t entityId) -> void;
        auto InvokeOnUpdate(uint64_t entityId, float timestep) -> void;
        auto InvokeOnDestroy(uint64_t entityId) -> void;

        auto SetFieldValue(uint64_t entityId, int32_t fieldIndex, const void* data) -> void;
        auto GetFieldValue(uint64_t entityId, int32_t fieldIndex, void* data) -> void;

    private:
        auto ResolveManagedFunctions() -> void;
        auto RebuildClassIndex() -> void;
        auto TakeManagedString(char* raw) const -> std::string;

    private:
        ScopedPtr<RuntimeHost> m_RuntimeHost = nullptr;
        ScopedPtr<ManagedFunctions> m_ManagedFns = nullptr;

        std::vector<ScriptClass> m_Classes;
    };
}
