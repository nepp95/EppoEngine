#include "pch.h"
#include "Scripting/Assembly.h"

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

        ResolveManagedFunctions();
        m_ManagedFns->Bootstrap();
        m_ManagedFns->RegisterNativeCallbacks(ScriptGlue::GetNativeCallbacks());
        RebuildClassIndex();
    }

    auto Assembly::LoadUserAssembly(const EP_NativeString& path) -> void
    {
        if (!m_ManagedFns)
        {
            Log::Error("No managed functions loaded!");
            return;
        }

        const auto pathStr = std::filesystem::path(path).string();
        m_ManagedFns->LoadUserAssembly(pathStr.c_str());
        RebuildClassIndex();
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

    auto Assembly::ResolveManagedFunctions() -> void
    {
        m_ManagedFns = CreateScopedPtr<ManagedFunctions>();

        auto ResolveFn = [&](const EP_NativeString& methodName) -> void*
        {
            const EP_NativeString managedAssemblyPath = EP_NATIVE_STR("EppoScriptCore.Managed.dll");
            const EP_NativeString scriptGlueType = EP_NATIVE_STR("EppoScriptCore.Core.ScriptGlue, EppoScriptCore.Managed");
            return m_RuntimeHost->GetManagedFnPointer(managedAssemblyPath, scriptGlueType, methodName);
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
        m_ManagedFns->SetFieldValue = reinterpret_cast<SetFieldValueFn>(ResolveFn(EP_NATIVE_STR("SetFieldValue")));
        m_ManagedFns->GetFieldValue = reinterpret_cast<GetFieldValueFn>(ResolveFn(EP_NATIVE_STR("GetFieldValue")));
        m_ManagedFns->FreeString = reinterpret_cast<FreeStringFn>(ResolveFn(EP_NATIVE_STR("FreeString")));
        m_ManagedFns->RegisterNativeCallbacks = reinterpret_cast<RegisterNativeCallbacksFn>(ResolveFn(EP_NATIVE_STR("RegisterNativeCallbacks")));
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
