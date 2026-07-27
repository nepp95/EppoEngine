#pragma once

#include "Scripting/ScriptGlue.h"
#include "Scripting/Platform.h"

namespace Eppo
{
    class RuntimeHost
    {
    public:
        explicit RuntimeHost(const EP_NativeString& runtimeConfigPath);
        ~RuntimeHost();

        RuntimeHost(const RuntimeHost&) = delete;
        RuntimeHost(RuntimeHost&&) = delete;
        RuntimeHost& operator=(const RuntimeHost&) = delete;
        RuntimeHost& operator=(RuntimeHost&&) = delete;

        [[nodiscard]] auto IsHostContextLoaded() const -> bool;

        auto
        GetManagedFnPointer(const EP_NativeString& assemblyPath, const EP_NativeString& typeName, const EP_NativeString& methodName) const
            -> void*;

    private:
        auto GetHostFxrPath() -> std::filesystem::path;
        auto LoadHostFxr(const std::filesystem::path& hostFxrPath) -> bool;
        auto InitializeRuntime(const EP_NativeString& runtimeConfigPath) -> bool;

    private:
        struct DotNetClrData;
        ScopedPtr<DotNetClrData> m_DotNetClrData;
    };
}
