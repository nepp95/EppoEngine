#pragma once

#include <string>

#if defined(EP_PLATFORM_WINDOWS)
    using EP_CharType = wchar_t;
    using EP_NativeString = std::wstring;

    #define EP_NATIVE_STR(s) L##s
    #define EP_HOSTFXR_NAME "hostfxr.dll"

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
    #include <ShlObj_core.h>

    #undef GetClassName
    using EP_LIBRARY = HMODULE;

    inline auto LoadLib(const EP_CharType* path) -> EP_LIBRARY
    {
        return LoadLibraryW(path);
    }

    inline auto GetSymbol(EP_LIBRARY lib, const char* name) -> void*
    {
        return reinterpret_cast<void*>(GetProcAddress(lib, name));
    }

#elif defined(EP_PLATFORM_LINUX)
    using EP_CharType = char;
    using EP_NativeString = std::string;

    #define EP_NATIVE_STR(s) s
    #define EP_HOSTFXR_NAME "libhostfxr.so"

    #include <dlfcn.h>
    using EP_LIBRARY = void*;

    inline auto LoadLib(const EP_CharType* path) -> EP_LIBRARY
    {
        return dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    }

    inline auto GetSymbol(EP_LIBRARY lib, const char* name) -> void*
    {
        return dlsym(lib, name);
    }

#else
    #error "Unsupported platform!"
#endif
