#pragma once

#include <cstdint>

namespace Eppo
{
    using BootstrapFn = void (*)();
    using GetClassCountFn = int32_t (*)();
    using GetClassNameFn = char* (*)(int32_t);
    using GetClassFieldCountFn = int32_t (*)(int32_t);
    using GetClassFieldNameFn = char* (*)(int32_t, int32_t);
    using GetClassFieldTypeFn = uint8_t (*)(int32_t, int32_t);
    using GetClassFieldDefaultValueFn = int32_t (*)(int32_t, int32_t, void*);
    using GetClassMethodCountFn = int32_t (*)(int32_t);
    using GetClassMethodNameFn = char* (*)(int32_t, int32_t);

    using LoadUserAssemblyFn = int32_t (*)(const char*);
    using UnloadUserAssemblyFn = void (*)();

    using CreateInstanceFn = int32_t (*)(int32_t classIndex, uint64_t entityId);
    using DestroyInstanceFn = void (*)(uint64_t entityId);
    using InvokeOnCreateFn = void (*)(uint64_t entityId);
    using InvokeOnUpdateFn = void (*)(uint64_t entityId, float deltaTime);
    using InvokeOnDestroyFn = void (*)(uint64_t entityId);
    using InvokeMethodFn = void (*)(uint64_t entityId, int32_t methodIndex, void* args, void* ret);

    using SetFieldValueFn = void (*)(uint64_t entityId, int32_t fieldIndex, void* data);
    using GetFieldValueFn = void (*)(uint64_t entityId, int32_t fieldIndex, void* data);

    using FreeStringFn = void (*)(char*);
    // Registers a native ScriptGlue function under the name managed calls it by.
    using RegisterInternalCallFn = void (*)(const char* name, void* function);

    struct ManagedFunctions
    {
        BootstrapFn Bootstrap = nullptr;

        GetClassCountFn GetClassCount = nullptr;
        GetClassNameFn GetClassName = nullptr;
        GetClassFieldCountFn GetClassFieldCount = nullptr;
        GetClassFieldNameFn GetClassFieldName = nullptr;
        GetClassFieldTypeFn GetClassFieldType = nullptr;
        GetClassFieldDefaultValueFn GetClassFieldDefaultValue = nullptr;
        GetClassMethodCountFn GetClassMethodCount = nullptr;
        GetClassMethodNameFn GetClassMethodName = nullptr;

        LoadUserAssemblyFn LoadUserAssembly = nullptr;
        UnloadUserAssemblyFn UnloadUserAssembly = nullptr;

        CreateInstanceFn CreateInstance = nullptr;
        DestroyInstanceFn DestroyInstance = nullptr;
        InvokeOnCreateFn InvokeOnCreate = nullptr;
        InvokeOnUpdateFn InvokeOnUpdate = nullptr;
        InvokeOnDestroyFn InvokeOnDestroy = nullptr;
        InvokeMethodFn InvokeMethod = nullptr;

        SetFieldValueFn SetFieldValue = nullptr;
        GetFieldValueFn GetFieldValue = nullptr;

        FreeStringFn FreeString = nullptr;
        RegisterInternalCallFn RegisterInternalCall = nullptr;
    };
}
