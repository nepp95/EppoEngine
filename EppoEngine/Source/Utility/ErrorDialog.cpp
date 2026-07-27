#include "pch.h"
#include "Utility/ErrorDialog.h"

#if defined(EP_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#elif defined(EP_PLATFORM_LINUX)
    #include "Utility/Process.h"
#endif

namespace Eppo::ErrorDialog
{
#if defined(EP_PLATFORM_WINDOWS)
    namespace
    {
        auto ToWide(const std::string_view text) -> std::wstring
        {
            if (text.empty())
                return {};
            const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring wide(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
            return wide;
        }
    }
#endif

    auto Show(const std::string_view title, const std::string_view message) -> void
    {
#if defined(EP_PLATFORM_WINDOWS)
        const auto wideTitle = ToWide(title);
        const auto wideMessage = ToWide(message);
        MessageBoxW(nullptr, wideMessage.c_str(), wideTitle.c_str(), MB_OK | MB_ICONERROR);
#elif defined(EP_PLATFORM_LINUX)
        RunProcess("zenity", { "--error", "--title=" + std::string(title), "--text=" + std::string(message) });
#endif
    }
}
