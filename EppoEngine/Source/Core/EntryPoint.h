#pragma once

#include "Core/Base.h"
#include "Core/Application.h"

namespace Eppo
{
    inline auto RunApplication(const int argc, char** argv) -> int
    {
        Log::Init();

        const ScopedPtr<Application> app(CreateApplication(argc, argv));
        EP_ASSERT(app != nullptr, "Application could not be created!");

        app->Run();

        return 0;
    }
}

#if !defined(EP_CUSTOM_ENTRY_POINT)
auto main(const int argc, char** argv) -> int
{
    return Eppo::RunApplication(argc, argv);
}
#endif
