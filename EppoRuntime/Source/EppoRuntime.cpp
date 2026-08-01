#include "RuntimeLayer.h"

#define EP_CUSTOM_ENTRY_POINT
#include <EppoEngine.h>
#include "Core/EntryPoint.h"

#if defined(EP_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#endif

namespace Eppo
{
    class RuntimeApplication final : public Application
    {
    public:
        explicit RuntimeApplication(ApplicationParams&& params, GameData gameData)
            : Application(std::move(params))
        {
            PushLayer<RuntimeLayer>(std::move(gameData));
        }
    };

    auto CreateApplication(int, char**) -> Application*
    {
        // Read here rather than in RunRuntime so it lands after RunApplication has started logging, and
        // before the application exists: its shaders are needed during startup. The layer takes the rest.
        GameData gameData;
        if (!gameData.Deserialize(FS::GetRootDirectory() / GameData::Filename))
            throw std::runtime_error("Game.eppak is missing, corrupt, or incompatible.");

        // Without these the engine would compile its shaders from a Resources directory the game does not ship.
        if (gameData.PackedShaders.empty())
            throw std::runtime_error("Game.eppak contains no engine shaders.");

        const auto title = FS::GetExecutablePath().stem().string();

        const CommandLineArgs args(0, nullptr);
        ApplicationParams params{
            .Args = args,
            .Title = title,
            .VSync = true,
            .Fullscreen = true,
            .EnableImGui =
#if defined(EP_DIST)
                false,
#else
                true,
#endif
            // Moved out: the renderer owns the shader text from here, and the layer has no use for it.
            .PackedShaders = std::move(gameData.PackedShaders),
            .PackedShaderIncludes = std::move(gameData.PackedShaderIncludes),
        };

        return new RuntimeApplication(std::move(params), std::move(gameData));
    }

    auto PrepareRuntimeStorage() -> bool
    {
        // Logs and the shader cache stay beside the game so its writes are visible in one place.
        if (!FS::ConfigureWritableDirectory(FS::GetExecutableDirectory()))
        {
            ErrorDialog::Show("Eppo Runtime Error", "Failed to create the runtime writable directory.");
            return false;
        }
        return true;
    }

    auto RunRuntime() -> int
    {
        // Everything runs inside the handler: creating the writable directory and reading the package both
        // touch the filesystem, and an unhandled throw here would terminate without a log or a dialog.
        try
        {
            if (!PrepareRuntimeStorage())
                return 1;

            return RunApplication(0, nullptr);
        }
        catch (const std::exception& exception)
        {
            Log::Error("Runtime failed: {}", exception.what());
            ErrorDialog::Show("Eppo Runtime Error", exception.what());
            return 1;
        }
    }
}

#if defined(EP_PLATFORM_WINDOWS)
auto WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int
{
    return Eppo::RunRuntime();
}
#else
auto main(int, char**) -> int
{
    return Eppo::RunRuntime();
}
#endif
