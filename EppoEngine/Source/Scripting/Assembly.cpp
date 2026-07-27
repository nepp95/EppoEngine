#include "pch.h"
#include "Scripting/Assembly.h"

#include "Scripting/ScriptGlue.h"

namespace Eppo
{
    Assembly::Assembly(const EP_NativeString& runtimeConfigPath)
    {
        m_RuntimeHost = CreateScopedPtr<RuntimeHost>(runtimeConfigPath);

        if (!m_RuntimeHost || !m_RuntimeHost->IsHostContextLoaded())
        {
            Log::Error("Assembly failed to initialize runtime!");
            return;
        }

        // EppoScriptCore.dll is deployed next to the runtime config; resolve it to
        // an absolute path so the load is independent of the working directory.
        m_CoreAssemblyPath = std::filesystem::path(runtimeConfigPath).parent_path() / "EppoScriptCore.dll";

        // If the managed functions don't resolve, leave m_ManagedFns null so every
        // call becomes a guarded no-op — never dereference a null function pointer.
        if (!ResolveManagedFunctions())
        {
            Log::Error(
                LogSource::Script, "Failed to resolve managed script functions from '{}'; scripts will be unavailable.",
                m_CoreAssemblyPath.string()
            );
            m_ManagedFns.reset();
            return;
        }

        m_ManagedFns->Bootstrap();
        RegisterInternalCalls();
        RebuildClassIndex();
    }

    auto Assembly::LoadUserAssembly(const EP_NativeString& path) -> bool
    {
        if (!m_ManagedFns)
        {
            Log::Error("No managed functions loaded!");
            return false;
        }

        const auto pathStr = std::filesystem::path(path).string();
        const bool loaded = m_ManagedFns->LoadUserAssembly(pathStr.c_str()) != 0;
        RebuildClassIndex();
        return loaded;
    }

    auto Assembly::UnloadUserAssembly() -> void
    {
        if (!m_ManagedFns)
        {
            Log::Error("No managed functions loaded!");
            return;
        }

        m_ManagedFns->UnloadUserAssembly();
        RebuildClassIndex();
    }

    auto Assembly::FindClassIndex(const std::string& fullName) const -> int32_t
    {
        for (int32_t i = 0; i < m_Classes.size(); i++)
        {
            if (m_Classes.at(i).GetFullName() == fullName)
                return i;
        }

        return -1;
    }

    auto Assembly::CreateInstance(const int32_t classIndex, const uint64_t entityId) const -> bool
    {
        if (!m_ManagedFns)
            return false;

        return m_ManagedFns->CreateInstance(classIndex, entityId) != 0;
    }

    auto Assembly::DestroyInstance(const uint64_t entityId) const -> void
    {
        if (m_ManagedFns)
            m_ManagedFns->DestroyInstance(entityId);
    }

    auto Assembly::InvokeOnCreate(const uint64_t entityId) const -> void
    {
        if (m_ManagedFns)
            m_ManagedFns->InvokeOnCreate(entityId);
    }

    auto Assembly::InvokeOnUpdate(const uint64_t entityId, const float timestep) const -> void
    {
        if (m_ManagedFns)
            m_ManagedFns->InvokeOnUpdate(entityId, timestep);
    }

    auto Assembly::InvokeOnDestroy(const uint64_t entityId) const -> void
    {
        if (m_ManagedFns)
            m_ManagedFns->InvokeOnDestroy(entityId);
    }

    auto Assembly::InvokeMethod(const uint64_t entityId, const int32_t methodIndex, const void* args, void* ret) const -> void
    {
        if (m_ManagedFns)
            // const_cast: the managed side only reads args; the C ABI param is non-const.
            m_ManagedFns->InvokeMethod(entityId, methodIndex, const_cast<void*>(args), ret);
    }

    auto Assembly::SetFieldValue(const uint64_t entityId, const int32_t fieldIndex, const void* data) const -> void
    {
        if (m_ManagedFns)
            // const_cast: the managed side only reads data; the C ABI param is non-const.
            m_ManagedFns->SetFieldValue(entityId, fieldIndex, const_cast<void*>(data));
    }

    auto Assembly::GetFieldValue(const uint64_t entityId, const int32_t fieldIndex, void* data) const -> void
    {
        if (m_ManagedFns)
            m_ManagedFns->GetFieldValue(entityId, fieldIndex, data);
    }

    auto Assembly::ResolveManagedFunctions() -> bool
    {
        m_ManagedFns = CreateScopedPtr<ManagedFunctions>();

        const EP_NativeString managedAssemblyPath = m_CoreAssemblyPath.native();
        const EP_NativeString scriptGlueType = EP_NATIVE_STR("EppoScriptCore.Core.ScriptGlue, EppoScriptCore");

        bool allResolved = true;
        auto ResolveFn = [&](const EP_NativeString& methodName) -> void*
        {
            void* fn = m_RuntimeHost->GetManagedFnPointer(managedAssemblyPath, scriptGlueType, methodName);
            if (!fn)
                allResolved = false;
            return fn;
        };

        m_ManagedFns->Bootstrap = reinterpret_cast<BootstrapFn>(ResolveFn(EP_NATIVE_STR("Bootstrap")));
        m_ManagedFns->GetClassCount = reinterpret_cast<GetClassCountFn>(ResolveFn(EP_NATIVE_STR("GetClassCount")));
        m_ManagedFns->GetClassName = reinterpret_cast<GetClassNameFn>(ResolveFn(EP_NATIVE_STR("GetClassName")));
        m_ManagedFns->GetClassFieldCount = reinterpret_cast<GetClassFieldCountFn>(ResolveFn(EP_NATIVE_STR("GetClassFieldCount")));
        m_ManagedFns->GetClassFieldName = reinterpret_cast<GetClassFieldNameFn>(ResolveFn(EP_NATIVE_STR("GetClassFieldName")));
        m_ManagedFns->GetClassFieldType = reinterpret_cast<GetClassFieldTypeFn>(ResolveFn(EP_NATIVE_STR("GetClassFieldType")));
        m_ManagedFns->GetClassMethodCount = reinterpret_cast<GetClassMethodCountFn>(ResolveFn(EP_NATIVE_STR("GetClassMethodCount")));
        m_ManagedFns->GetClassMethodName = reinterpret_cast<GetClassMethodNameFn>(ResolveFn(EP_NATIVE_STR("GetClassMethodName")));
        m_ManagedFns->LoadUserAssembly = reinterpret_cast<LoadUserAssemblyFn>(ResolveFn(EP_NATIVE_STR("LoadUserAssembly")));
        m_ManagedFns->UnloadUserAssembly = reinterpret_cast<UnloadUserAssemblyFn>(ResolveFn(EP_NATIVE_STR("UnloadUserAssembly")));
        m_ManagedFns->CreateInstance = reinterpret_cast<CreateInstanceFn>(ResolveFn(EP_NATIVE_STR("CreateInstance")));
        m_ManagedFns->DestroyInstance = reinterpret_cast<DestroyInstanceFn>(ResolveFn(EP_NATIVE_STR("DestroyInstance")));
        m_ManagedFns->InvokeOnCreate = reinterpret_cast<InvokeOnCreateFn>(ResolveFn(EP_NATIVE_STR("InvokeOnCreate")));
        m_ManagedFns->InvokeOnUpdate = reinterpret_cast<InvokeOnUpdateFn>(ResolveFn(EP_NATIVE_STR("InvokeOnUpdate")));
        m_ManagedFns->InvokeOnDestroy = reinterpret_cast<InvokeOnDestroyFn>(ResolveFn(EP_NATIVE_STR("InvokeOnDestroy")));
        m_ManagedFns->InvokeMethod = reinterpret_cast<InvokeMethodFn>(ResolveFn(EP_NATIVE_STR("InvokeMethod")));
        m_ManagedFns->SetFieldValue = reinterpret_cast<SetFieldValueFn>(ResolveFn(EP_NATIVE_STR("SetFieldValue")));
        m_ManagedFns->GetFieldValue = reinterpret_cast<GetFieldValueFn>(ResolveFn(EP_NATIVE_STR("GetFieldValue")));
        m_ManagedFns->FreeString = reinterpret_cast<FreeStringFn>(ResolveFn(EP_NATIVE_STR("FreeString")));
        m_ManagedFns->RegisterInternalCall = reinterpret_cast<RegisterInternalCallFn>(ResolveFn(EP_NATIVE_STR("RegisterInternalCall")));

        return allResolved;
    }

    auto Assembly::RegisterInternalCalls() const -> void
    {
        for (const auto& [name, function] : ScriptGlue::GetInternalCalls())
            m_ManagedFns->RegisterInternalCall(name, function);
    }

    auto Assembly::RebuildClassIndex() -> void
    {
        m_Classes.clear();

        if (!m_ManagedFns)
        {
            Log::Error("No managed functions loaded!");
            return;
        }

        const auto count = m_ManagedFns->GetClassCount();
        for (int32_t i = 0; i < count; i++)
        {
            const auto fullName = TakeManagedString(m_ManagedFns->GetClassName(i));
            if (fullName.empty())
                continue;

            const auto lastDot = fullName.find_last_of('.');
            const auto nameSpace = (lastDot != std::string::npos) ? fullName.substr(0, lastDot) : "";
            const auto name = (lastDot != std::string::npos) ? fullName.substr(lastDot + 1) : fullName;

            ScriptClass cls(fullName, nameSpace, name);

            const auto fieldCount = m_ManagedFns->GetClassFieldCount(i);
            for (int32_t j = 0; j < fieldCount; j++)
            {
                const auto fieldName = TakeManagedString(m_ManagedFns->GetClassFieldName(i, j));
                if (fieldName.empty())
                    continue;

                ScriptField field{
                    .Name = fieldName,
                    .Type = static_cast<ScriptFieldType>(m_ManagedFns->GetClassFieldType(i, j)),
                };

                cls.m_Fields.emplace_back(std::move(field));
            }

            const auto methodCount = m_ManagedFns->GetClassMethodCount(i);
            for (int32_t j = 0; j < methodCount; j++)
            {
                const auto methodName = TakeManagedString(m_ManagedFns->GetClassMethodName(i, j));
                if (methodName.empty())
                    continue;

                ScriptMethod method{
                    .Name = methodName,
                    .Index = j,
                };

                cls.m_Methods.emplace_back(std::move(method));
            }

            m_Classes.emplace_back(std::move(cls));
        }
    }

    auto Assembly::TakeManagedString(char* raw) const -> std::string
    {
        if (!raw)
            return {};

        std::string value(raw);
        m_ManagedFns->FreeString(raw);
        return value;
    }
}
