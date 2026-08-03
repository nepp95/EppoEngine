#include "pch.h"
#include "Scripting/RuntimeHost.h"

#include <coreclr_delegates.h>
#include <hostfxr.h>

namespace Eppo
{
    struct RuntimeHost::DotNetClrData
    {
        void* HostFxrLib = nullptr;
        hostfxr_handle HostContext = nullptr;
        hostfxr_initialize_for_runtime_config_fn InitFn = nullptr;
        hostfxr_get_runtime_delegate_fn GetDelegateFn = nullptr;
        hostfxr_close_fn CloseFn = nullptr;
        hostfxr_set_error_writer_fn ErrorWriterFn = nullptr;
    };

    RuntimeHost::RuntimeHost(const EP_NativeString& runtimeConfigPath)
    {
        const auto hostFxrPath = GetHostFxrPath();
        if (hostFxrPath.empty())
        {
            Log::Error(LogSource::Script, "Failed to locate hostfxr!");
            return;
        }

        if (!LoadHostFxr(hostFxrPath))
        {
            Log::Error(LogSource::Script, "Failed to load hostfxr! ({})", hostFxrPath);
            return;
        }

        if (!InitializeRuntime(runtimeConfigPath))
        {
            Log::Error(LogSource::Script, "Failed to initialize dotnet runtime!");
            return;
        }
    }

    RuntimeHost::~RuntimeHost()
    {
        if (m_DotNetClrData && m_DotNetClrData->HostContext && m_DotNetClrData->CloseFn)
            m_DotNetClrData->CloseFn(m_DotNetClrData->HostContext);
    }

    auto RuntimeHost::IsHostContextLoaded() const -> bool
    {
        return m_DotNetClrData && m_DotNetClrData->HostContext;
    }

    auto RuntimeHost::GetManagedFnPointer(
        const EP_NativeString& assemblyPath, const EP_NativeString& typeName, const EP_NativeString& methodName
    ) const -> void*
    {
        if (!IsHostContextLoaded())
        {
            Log::Error(LogSource::Script, "RuntimeHost not initialized!");
            return nullptr;
        }

        load_assembly_and_get_function_pointer_fn loadFn = nullptr;
        void* methodPtr = nullptr;

        if (const int result = m_DotNetClrData->GetDelegateFn(
                m_DotNetClrData->HostContext, hdt_load_assembly_and_get_function_pointer, reinterpret_cast<void**>(&loadFn)
            );
            result != 0 || !loadFn)
        {
            Log::Error(
                LogSource::Script, "{}", std::format("hostfxr_get_runtime_delegate failed: 0x{:08X}", static_cast<uint32_t>(result))
            );
            return nullptr;
        }

        if (const int result =
                loadFn(assemblyPath.c_str(), typeName.c_str(), methodName.c_str(), UNMANAGEDCALLERSONLY_METHOD, nullptr, &methodPtr);
            result != 0 || !methodPtr)
        {
            Log::Error(
                LogSource::Script, "{}",
                std::format("load_assembly_and_get_function_pointer failed: 0x{:08X}", static_cast<uint32_t>(result))
            );
            return nullptr;
        }

        return methodPtr;
    }

    auto RuntimeHost::GetHostFxrPath() -> std::filesystem::path
    {
        auto GetDotNetRoot = []() -> std::filesystem::path
        {
            if (const auto* envRoot = std::getenv("DOTNET_ROOT"))
                return envRoot;

#if defined(EP_PLATFORM_WINDOWS)
            TCHAR programFiles[MAX_PATH];
            SHGetSpecialFolderPath(nullptr, programFiles, CSIDL_PROGRAM_FILES, FALSE);
            return std::filesystem::path(programFiles) / "dotnet";
#elif defined(EP_PLATFORM_LINUX)
            return "/usr/share/dotnet";
#else
    #error "Unsupported platform!"
#endif
        };

        const auto dotnetRoot = GetDotNetRoot();
        const auto fxrRoot = dotnetRoot / "host" / "fxr";

        if (!FS::Exists(fxrRoot))
            return {};

        std::filesystem::path libraryPath;
        for (const auto& entry : std::filesystem::directory_iterator(fxrRoot))
        {
            if (!entry.is_directory())
                continue;

            const auto candidate = entry.path() / EP_HOSTFXR_NAME;
            if (!FS::Exists(candidate))
                continue;

            if (libraryPath.empty() || entry.path().filename().string() > libraryPath.parent_path().filename().string())
                libraryPath = candidate;
        }

        return libraryPath;
    }

    auto RuntimeHost::LoadHostFxr(const std::filesystem::path& hostFxrPath) -> bool
    {
        m_DotNetClrData = std::make_unique<DotNetClrData>();
        m_DotNetClrData->HostFxrLib = LoadLib(hostFxrPath.c_str());

        if (!m_DotNetClrData->HostFxrLib)
        {
            Log::Error(LogSource::Script, "Failed to load hostfxr!");
            return false;
        }

        m_DotNetClrData->InitFn = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            GetSymbol(static_cast<EP_LIBRARY>(m_DotNetClrData->HostFxrLib), "hostfxr_initialize_for_runtime_config")
        );
        m_DotNetClrData->GetDelegateFn = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            GetSymbol(static_cast<EP_LIBRARY>(m_DotNetClrData->HostFxrLib), "hostfxr_get_runtime_delegate")
        );
        m_DotNetClrData->CloseFn =
            reinterpret_cast<hostfxr_close_fn>(GetSymbol(static_cast<EP_LIBRARY>(m_DotNetClrData->HostFxrLib), "hostfxr_close"));
        m_DotNetClrData->ErrorWriterFn = reinterpret_cast<hostfxr_set_error_writer_fn>(
            GetSymbol(static_cast<EP_LIBRARY>(m_DotNetClrData->HostFxrLib), "hostfxr_set_error_writer")
        );

        return true;
    }

    auto RuntimeHost::InitializeRuntime(const EP_NativeString& runtimeConfigPath) -> bool
    {
        // Success return codes
        constexpr auto Success = 0;
        constexpr auto Success_HostAlreadyInitialized = 0x00001;
        constexpr auto Success_DifferentRuntimeProperties = 0x00002;

        void* hostContext = nullptr;
        const auto result = m_DotNetClrData->InitFn(runtimeConfigPath.c_str(), nullptr, &hostContext);

        if (result == Success_HostAlreadyInitialized || result == Success_DifferentRuntimeProperties)
        {
            m_DotNetClrData->HostContext = hostContext;
            return true;
        }

        if (result != Success || hostContext == nullptr)
        {
            Log::Error(
                LogSource::Script, "{}",
                std::format("hostfxr_initialize_for_runtime_config failed: 0x{:08X}", static_cast<uint32_t>(result))
            );
            return false;
        }

        m_DotNetClrData->HostContext = hostContext;
        return true;
    }
}
