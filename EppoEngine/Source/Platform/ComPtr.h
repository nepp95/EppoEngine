#pragma once

// clang-format off
#if defined(EP_PLATFORM_WINDOWS)
    #include <wrl/client.h>
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
#else
    #include <dxc/WinAdapter.h>
    template<typename T>
    using ComPtr = CComPtr<T>;
#endif
// clang-format on
