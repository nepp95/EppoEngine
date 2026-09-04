#pragma once

#if defined(EP_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include "Platform/ComPtr.h"

    #include <d3d12.h>
    #include <dxgi1_6.h>

namespace Eppo
{
    #define DX_CHECK(fn, msg)                                                                                                              \
        if (const HRESULT result__ = (fn); FAILED(result__))                                                                               \
        {                                                                                                                                  \
            Log::Error(LogSource::DX12, "{} (HRESULT 0x{:08X})", msg, static_cast<uint32_t>(result__));                                    \
            EP_ASSERT(false, msg);                                                                                                         \
        }
}

#endif
